/*
 * @file    LEDfifoLKM.c
 * @author  Stephen M Moraco
 * @date    15 November 2019
 * @version 0.1
 * @brief  An introductory "Hello World!" loadable kernel module (LKM) that can display a message
 * in the /var/log/kern.log file when the module is loaded and removed. The module can accept an
 * argument when it is loaded -- the name, which appears in the kernel log files.
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.


- History:
-   Round 1 direct I/O
-   Round 2 let's awaken mem/map access...
*/

#include <linux/init.h>             // Macros used to mark up functions e.g., __init __exit
#include <linux/module.h>           // Core header for loading LKMs into the kernel
#include <linux/kernel.h>           // Contains types, macros, functions for the kernel
#include <linux/version.h>
#include <linux/types.h>            // for dev_t
#include <linux/kdev_t.h>           // foir registering device Maj/Min
#include <linux/fs.h>               // for *_chrdev_region() calls
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/string.h>           // for strxxx() and memxxx() calls
#include <linux/uaccess.h>          // copy_*_user
#include <linux/proc_fs.h>          // proc filesystem support
#include <linux/jiffies.h>
#include <linux/fcntl.h>	        // O_ACCMODE
#include <linux/slab.h>		        // kmalloc()
#include <linux/errno.h>	        // error codes
#include <linux/seq_file.h>
#include <linux/spinlock.h>

// get raspbery PI details
#include <asm/io.h>
//#include <mach/platform.h>
//#include <linux/delay.h>
#include <linux/interrupt.h>    // for tasklets


#include "LEDfifoConfigureIOCtl.h"


// ----------------------------------------------------------------------------
//  SECTION: LINUX KERNEL MODULE constants & Parameter Def's
//
MODULE_LICENSE("GPL");              ///< The license type -- this affects runtime behavior
MODULE_AUTHOR("Stephen M Moraco");      ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("An LED Matrix display GPIO Driver.");  ///< The description -- see modinfo
MODULE_VERSION("0.1");              ///< The version of the module

static char *name = "{nameParm}";        ///< An example LKM argument -- default value is "{nameParm}"
module_param(name, charp, S_IRUGO); ///< Param desc. charp = char ptr, S_IRUGO can be read/not changed
MODULE_PARM_DESC(name, "The name to display in /var/log/kern.log");  ///< parameter description


// ----------------------------------------------------------------------------
//  SECTION: File-scoped Constants/Macros
//
#define LED_FIFO_MAJOR 0   /* dynamic major by default */
#define LED_FIFO_NR_DEVS 1    /* ledfifo0  (not ledfifo0-ledfifoN) */

#define DEFAULT_LED_STRTYPE "WS2812B"
#define DEFAULT_PERIOD_IN_NSEC 49
#define DEFAULT_PERIOD_COUNT 26
#define DEFAULT_T0H_COUNT 8
#define DEFAULT_T1H_COUNT 17
#define DEFAULT_TRESET_COUNT 1020
#define DEFAULT_LOOP_ENABLE 0

// our LED Matrix dimensions
#define HARDWARE_MAX_PANELS 3
#define HARDWARE_MAX_LEDS_PER_PANEL 256
#define HARDWARE_MAX_COLOR_BYTES_PER_LED 3


#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
#define STR_PRINTF_RET(len, str, args...) len += sprintf(page + len, str, ## args)
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0))
#define STR_PRINTF_RET(len, str, args...) len += seq_printf(m, str, ## args)
#else
#define STR_PRINTF_RET(len, str, args...) seq_printf(m, str, ## args)
#endif

// forward declarations
static long LEDfifo_ioctl(struct file *f, unsigned int cmd, unsigned long arg);
static void init_gpio_access(void);
//static void init_timer_access(void);
static void init_interrupt_access(void);
static void resetCurrentPins(void);
static void initCurrentPins(void);
static void initBitTableForCurrentPins(void);
static void xmitBitValuesToAllChannels(uint8_t bitsIndex);
static void testXmitZeros(uint32_t nCount);
static void testXmitOnes(uint32_t nCount);
static void testXmitBit(uint16_t onDelay, uint16_t offDelay);

static void dumpPinTable(void);
static void hexDump(const char message[], const char *addr, const int len);

static void configureDriverIO(uint32_t baseAddress);
int interrupts(int bDisableRequest);
void taskletTestWrites(unsigned long data);
void taskletScreenFill(unsigned long data);
void taskletScreenWrite(unsigned long data);

void nSecDelay(int nSecDuration);
#define ndelay nSecDelay

// ----------------------------------------------------------------------------
//  SECTION: File-scoped Global Variables
//
static dev_t firstDevNbr; // Global variable for the first device number
static struct cdev c_dev; // Global variable for the character device structure
static struct class *cl; // Global variable for the device class

static struct proc_dir_entry *parent;
static struct proc_dir_entry *file;

enum pitypes {NOTSET,ARM6,ARM7,PI4};
static enum pitypes s_ePiType = NOTSET;  // set by identifyPiModel()  0=not set 1=ARMv6  2=ARMv7, etc.

static uint32_t s_pRPiModelIOBaseAddress;
static volatile unsigned int *gpio;

static uint8_t *kernel_buffer;
static size_t s_screenBufferSizeInBytes = (HARDWARE_MAX_PANELS * HARDWARE_MAX_LEDS_PER_PANEL * HARDWARE_MAX_COLOR_BYTES_PER_LED);

static unsigned char ledType[FIFO_MAX_STR_LEN+1] = DEFAULT_LED_STRTYPE; // +1 for zero term.
static int gpioPins[FIFO_MAX_PIN_COUNT];    // max 3 gpio pins can be assigned
static int periodDurationNsec = DEFAULT_PERIOD_IN_NSEC;
static int periodCount = DEFAULT_PERIOD_COUNT;
static int periodT0HCount = DEFAULT_T0H_COUNT;
static int periodT1HCount = DEFAULT_T1H_COUNT;
static int periodTRESETCount = DEFAULT_TRESET_COUNT;
static int loopEnabled = DEFAULT_LOOP_ENABLE;

static struct tasklet_struct tasklet;

// ----------------------------------------------------------------------------
//  SECTION: file-I/O handlers
//
static int LEDfifo_open(struct inode *i, struct file *f)
{
    // alloc our single screen buffer the user will write to...
    if((kernel_buffer = kmalloc(s_screenBufferSizeInBytes , GFP_KERNEL)) == 0){
        printk(KERN_ERR "LEDfifo: open() Cannot allocate Screen Buffer(s) in kernel\n");
        return -1;
    }
    printk(KERN_INFO "LEDfifo: open() w/Alloc Screen Buffer(s)\n");
    return 0;
}


static int LEDfifo_close(struct inode *i, struct file *f)
{
    kfree(kernel_buffer);
    printk(KERN_INFO "LEDfifo: close() released Screen Buffer(s)\n");
    return 0;
}


static ssize_t LEDfifo_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    printk(KERN_INFO "LEDfifo: read()\n");
    return 0;
}


static ssize_t LEDfifo_write(struct file *f, const char __user *buf, size_t len,
    loff_t *off)
{
    unsigned long bytesNotCopied;

    printk(KERN_INFO "LEDfifo: write(%d) bytes\n", len);
    bytesNotCopied = len;

    if(s_ePiType == NOTSET) {
        printk(KERN_ERR "LEDfifo: write() Abort, RPi Model not yet identified! (IO not configured!)\n");
    }
    else {
        if(len > s_screenBufferSizeInBytes) {
            printk(KERN_ERR "LEDfifo: write() Abort, too long (%ld bytes) [> max %d]\n", bytesNotCopied, s_screenBufferSizeInBytes);
        }
        else {
            bytesNotCopied = copy_from_user(kernel_buffer, buf, len);
            if(bytesNotCopied != 0) {
                printk(KERN_ERR "LEDfifo: write() Failed to copy %ld bytes in kernel\n", bytesNotCopied);
            }
            else {
                // write buffer via GPIO to matrix
                // FIXME: UNDONE maybe pass desired buffer ptr as data? at task init
                tasklet_init(&tasklet, taskletScreenWrite, 0);
                tasklet_hi_schedule(&tasklet);
            }
        }
    }
    return len - bytesNotCopied;
}

/*
static ssize_t LEDfifo_readv(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    printk(KERN_INFO "LEDfifo: readv()\n");
    return 0;
}


static ssize_t LEDfifo_writev(struct file *f, const char __user *buf, size_t len,
    loff_t *off)
{
    printk(KERN_INFO "LEDfifo: writev()\n");
    return len;
}
*/

static struct file_operations LEDfifoLKM_fops =
{
    .owner = THIS_MODULE,
    .open = LEDfifo_open,
    .read = LEDfifo_read,
    .write = LEDfifo_write,
//    .readv = LEDfifo_readv,
//    .writev = LEDfifo_writev,
     .release = LEDfifo_close,
    .unlocked_ioctl = LEDfifo_ioctl
};


// ----------------------------------------------------------------------------
//  SECTION: IO Control (IOCTL) handlers
//
static long LEDfifo_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    configure_arg_t cfg;
    long retval = 0;  // default to returning success
    int err = 0;
    int pinIndex;


    /*
    * extract the type and number bitfields, and don't decode
    * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
    */
    if (_IOC_TYPE(cmd) != LED_FIFO_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > LED_FIFO_IOC_MAXNR)
        return -ENOTTY;

    /*
    * the direction is a bitmask, and VERIFY_WRITE catches R/W * transfers.
    * `Type' is user-oriented, while access_ok is kernel-oriented, so the
    * concept of "read" and * "write" is reversed
    */
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    if ((!err) && (_IOC_DIR(cmd) & _IOC_WRITE))
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    if (err)
        return -EFAULT;

    /*
    * now handle the legitimate command
    */
    switch (cmd)
    {
        case CMD_GET_VARIABLES:
            printk(KERN_INFO "LEDfifo: ioctl() get variables\n");
            memset(cfg.ledType, 0, FIFO_MAX_STR_LEN+1);
            strncpy(cfg.ledType, ledType, FIFO_MAX_STR_LEN);
            for(pinIndex = 0; pinIndex < FIFO_MAX_PIN_COUNT; pinIndex++) {
                cfg.gpioPins[pinIndex] = gpioPins[pinIndex];
            }
            cfg.periodDurationNsec = periodDurationNsec;
            cfg.periodCount = periodCount;
            cfg.periodT0HCount = periodT0HCount;
            cfg.periodT1HCount = periodT1HCount;
            cfg.periodTRESETCount = periodTRESETCount;
            // copy_to_user(to,from,count)
            if (copy_to_user((configure_arg_t *)arg, &cfg,
                sizeof(configure_arg_t)))
            {
                return -EACCES;
            }
            break;
        case CMD_SET_VARIABLES:
            printk(KERN_INFO "LEDfifo: ioctl() set variables\n");
            // if any prior pins selected reset them to INPUT
            resetCurrentPins();

            // copy_from_user(to,from,count)
            if (copy_from_user(&cfg, (configure_arg_t *)arg, sizeof(configure_arg_t))) {
                return -EACCES;
            }
            memset(ledType, 0, FIFO_MAX_STR_LEN+1);
            strncpy(ledType, cfg.ledType, FIFO_MAX_STR_LEN);
            for(pinIndex = 0; pinIndex < FIFO_MAX_PIN_COUNT; pinIndex++) {
                gpioPins[pinIndex] = cfg.gpioPins[pinIndex];
            }
            periodDurationNsec = cfg.periodDurationNsec;
            periodCount = cfg.periodCount;
            periodT0HCount = cfg.periodT0HCount;
            periodT1HCount = cfg.periodT1HCount;
            periodTRESETCount = cfg.periodTRESETCount;

            // if we now have pins configure them and load our bit-send table
            initCurrentPins();
            initBitTableForCurrentPins();

            break;
        case CMD_RESET_VARIABLES:
            printk(KERN_INFO "LEDfifo: ioctl() - reset variables\n");
            // reconfigure for WS2812B
            memset(ledType, 0, FIFO_MAX_STR_LEN+1);
            strncpy(ledType, DEFAULT_LED_STRTYPE, FIFO_MAX_STR_LEN);
            for(pinIndex = 0; pinIndex < FIFO_MAX_PIN_COUNT; pinIndex++) {
                gpioPins[pinIndex] = 0;
            }
            periodDurationNsec = DEFAULT_PERIOD_IN_NSEC;
            periodCount = DEFAULT_PERIOD_COUNT;
            periodT0HCount = DEFAULT_T0H_COUNT;
            periodT1HCount = DEFAULT_T1H_COUNT;
            periodTRESETCount = DEFAULT_TRESET_COUNT;
            break;
        case CMD_SET_LOOP_ENABLE:
            printk(KERN_INFO "LEDfifo: ioctl() set loop enable=%ld\n", arg);
            loopEnabled = arg;
            break;
        case CMD_GET_LOOP_ENABLE:
            printk(KERN_INFO "LEDfifo: ioctl() get loop enable: return (%d)\n", loopEnabled);
            retval = loopEnabled;
            break;
        case CMD_TEST_BIT_WRITES:
            printk(KERN_INFO "LEDfifo: ioctl() test bit writes w/(%ld's)\n", arg);
            if(s_ePiType == NOTSET) {
                printk(KERN_ERR "LEDfifo: ioctl() Abort, RPi Model not yet identified! (IO not configured!)\n");
            }
            else {
                if(arg == 0) {
                    //testXmitZeros(1000);
                    tasklet_init(&tasklet, taskletTestWrites, 0);
                    tasklet_hi_schedule(&tasklet);
                }
                else {
                    //testXmitOnes(1000);
                    tasklet_init(&tasklet, taskletTestWrites, 1);
                    tasklet_hi_schedule(&tasklet);
               }
           }
            break;
        case CMD_CLEAR_SCREEN:
            printk(KERN_INFO "LEDfifo: ioctl() clear screen: set screen color 0x%06X\n", 0);
            if(s_ePiType == NOTSET) {
                printk(KERN_ERR "LEDfifo: ioctl() Abort, RPi Model not yet identified! (IO not configured!)\n");
            }
            else {
                tasklet_init(&tasklet, taskletScreenFill, 0);
                tasklet_hi_schedule(&tasklet);
            }
            break;
        case CMD_SET_SCREEN_COLOR:
            printk(KERN_INFO "LEDfifo: ioctl() set screen color 0x%06lX\n", arg);
            if(s_ePiType == NOTSET) {
                printk(KERN_ERR "LEDfifo: ioctl() Abort, RPi Model not yet identified! (IO not configured!)\n");
            }
            else {
                tasklet_init(&tasklet, taskletScreenFill, arg);
                tasklet_hi_schedule(&tasklet);
            }
            break;
        case CMD_SET_IO_BASE_ADDRESS:
            printk(KERN_INFO "LEDfifo: ioctl() set rpiBaseAddr 0x%08lX\n", arg);
            s_pRPiModelIOBaseAddress = arg;
            configureDriverIO(arg);
            break;
        default:
            printk(KERN_WARNING "LEDfifo: ioctl() unknown command (%d) !!\n", cmd);
            return -EINVAL; // unknown command?  How'd this happen?
            break;
    }

    return retval;
}


// ----------------------------------------------------------------------------
//  SECTION: /proc/ filesystem handlers
//
static int config_read(struct seq_file *m, void *v)
{
    int len = 0;
    //int freqInKHz;
    int pinIndex;
    unsigned char *loopStatus;

    STR_PRINTF_RET(len, "LED String Type: %s\n", ledType);
    STR_PRINTF_RET(len, "GPIO Pins Assigned:\n");
    for(pinIndex = 0; pinIndex < FIFO_MAX_PIN_COUNT; pinIndex++) {
        if(gpioPins[pinIndex] != 0) {
        	STR_PRINTF_RET(len, " - #%d - GPIO %d\n", pinIndex+1, gpioPins[pinIndex]);
    	}
    	else {
            	STR_PRINTF_RET(len, " - #%d - {not set}\n", pinIndex+1);
    	}
    }
    STR_PRINTF_RET(len, "Serial Stream: %d nSec Period (%d x %d nSec increments)\n", (periodCount * periodDurationNsec), periodCount,  periodDurationNsec);
    STR_PRINTF_RET(len, "        Bit0: Hi %d nSec -> Lo %d nSec\n", periodT0HCount * periodDurationNsec, (periodCount - periodT0HCount) * periodDurationNsec);
    STR_PRINTF_RET(len, "        Bit1: Hi %d nSec -> Lo %d nSec\n", periodT1HCount * periodDurationNsec, (periodCount - periodT1HCount) * periodDurationNsec);
    STR_PRINTF_RET(len, "       Reset: Lo %d nSec\n", (periodTRESETCount * periodDurationNsec));
    STR_PRINTF_RET(len, "\n");
    loopStatus = (loopEnabled) ? "YES" : "no";
    STR_PRINTF_RET(len, "  Looping Enabled: %s\n", loopStatus);
    STR_PRINTF_RET(len, "\n");

    return len;
}

static int config_open(struct inode *inode, struct file *file)
{
    return single_open(file, config_read, NULL);
}

static struct file_operations proc_fops =
{
    .owner = THIS_MODULE,
    .open = config_open,
    .read = seq_read
};


// ----------------------------------------------------------------------------
//  SECTION: LINUX KERNEL MODULE init/exit
//
/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init LEDfifoLKM_init(void){
    int ret;
    struct device *dev_ret;

    printk(KERN_INFO "LEDfifo: init(%s) ENTRY\n", name);

    printk(KERN_INFO "LEDfifo: ofcd register");
    if ((ret = alloc_chrdev_region(&firstDevNbr, LED_FIFO_MAJOR, LED_FIFO_NR_DEVS, "ledfifo")) < 0)
    {
        printk(KERN_WARNING "LEDfifo: can't alloc major\n");
        return ret;
    }
    printk(KERN_INFO "LEDfifo: <Major, Minor>: <%d, %d> (dev_t=0x%8X)\n", MAJOR(firstDevNbr), MINOR(firstDevNbr), firstDevNbr);

    if (IS_ERR(cl = class_create(THIS_MODULE, "ledfifo")))      // should find this in /sys/class/ledfifo
    {
        unregister_chrdev_region(firstDevNbr, 1);
        return PTR_ERR(cl);
    }
    if (IS_ERR(dev_ret = device_create(cl, NULL, firstDevNbr, NULL, "ledfifo")))
    {
        class_destroy(cl);
        unregister_chrdev_region(firstDevNbr, 1);
        return PTR_ERR(dev_ret);
    }

    printk(KERN_INFO "LEDfifo: c_dev add\n");
    cdev_init(&c_dev, &LEDfifoLKM_fops);
    if ((ret = cdev_add(&c_dev, firstDevNbr, 1)) < 0)
    {
        device_destroy(cl, firstDevNbr);
        class_destroy(cl);
        unregister_chrdev_region(firstDevNbr, 1);
        return ret;
    }

    // and now our proc entries (fm Chap16)
    printk(KERN_INFO "LEDfifo: /proc/driver add\n");
    if ((parent = proc_mkdir("driver/ledfifo", NULL)) == NULL)
    {
        return -1;
    }
    if ((file = proc_create("config", 0444, parent, &proc_fops)) == NULL)
    {
        remove_proc_entry("driver/ledfifo", NULL);
        return -1;
    }

    printk(KERN_INFO "LEDfifo: init EXIT\n");

    return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit LEDfifoLKM_exit(void){
    printk(KERN_INFO "LEDfifo: Exit(%s)\n", name);

    /* release the mapping */
    printk(KERN_INFO "LEDfifo: : release gpio io-remap\n");
    iounmap((void *)gpio);

    // fm Chap16
    remove_proc_entry("config", parent);
    remove_proc_entry("driver/ledfifo", NULL);

    // fm Chap5
    cdev_del(&c_dev);
    device_destroy(cl, firstDevNbr);
    class_destroy(cl);

    // fm Chap4
    printk(KERN_INFO "LEDfifo: (dev_t=0x%8X)\n", firstDevNbr);
    unregister_chrdev_region(firstDevNbr, 1);

    printk(KERN_INFO "LEDfifo: ofcd unregistered\n");
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(LEDfifoLKM_init);
module_exit(LEDfifoLKM_exit);


// ----------------------------------------------------------------------------
//  SECTION: Backend GPIO Handlers
//
struct GpioRegisters {
    uint32_t GPFSEL[6];
    uint32_t Reserved1;
    uint32_t GPSET[2];
    uint32_t Reserved2;
    uint32_t GPCLR[2];
}; //  __attribute__((packed));

//struct GpioRegisters *s_pGpioRegisters = (struct GpioRegisters *)__io_address(GPIO_BASE);

struct GpioRegisters volatile *s_pGpioRegisters;



// RPi 1
#define RPI1_BCM2708_PERI_BASE        0x20000000

// RPi 2
#define RPI2_BCM2708_PERI_BASE        0x3F000000

// RPi 3 (same as 2!)
//#define RPI3_BCM2708_PERI_BASE        0x3F000000

// RPi 4
#define RPI4_BCM2708_PERI_BASE        0xFE000000

#define GPIO_OFFSET 0x00200000
#define TIMER_OFFSET 0x00003000
#define INT_OFFSET 0x0000B000

#define RPI4_GIC_distributor 0xFF841000
#define RPI2_3_CORES_BASE 0x40000000

// GPIO controller
//#define GPIO_BASE                (BCM2708_PERI_BASE + GPIO_OFFSET)
#define GPIO_BLOCK_SIZE (4*1024)
#define INTR_BLOCK_SIZE (4*1024)
#define CORES_BLOCK_SIZE (4*1024)
#define TIMER_BLOCK_SIZE (4*1024)

/*
**
**  Old stuff: here for reference...
**
// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define SET_GPIO(g) (*(gpio+7) = 1<<(g))
#define CLR_GPIO(g) (*(gpio+10) = 1<<(g))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH

#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock
*/
/*

#define GPIO_FSEL_REG0 (&gpio[0])
#define GPIO_FSEL_REG1 (&gpio[1])
#define GPIO_SET_REG0  (&gpio[7])
#define GPIO_SET_REG1  (&gpio[8])
#define GPIO_CLR_REG0  (&gpio[10])
#define GPIO_CLR_REG1  (&gpio[11])

// -OR-

#define GPREG_FSEL0 0
#define GPREG_FSEL1 1
#define GPREG_SET0  7
#define GPREG_SET1  8
#define GPREG_CLR0 10
#define GPREG_CLR1 10
#define GPIO_ADDR(reg) (&gpio[reg])

#define RPI1_BCM2708_PERI_BASE        0x20000000

// RPi 2
#define RPI2_BCM2708_PERI_BASE        0x3F000000

// RPi 3 (same as 2!)
//#define RPI3_BCM2708_PERI_BASE        0x3F000000

// RPi 4
#define RPI4_BCM2708_PERI_BASE        0xFE000000

*/

static void configureDriverIO(uint32_t baseAddress)
{
    s_pRPiModelIOBaseAddress = baseAddress;

    switch(s_pRPiModelIOBaseAddress) {
        case RPI1_BCM2708_PERI_BASE:
            s_ePiType = ARM6;
            break;
        case RPI2_BCM2708_PERI_BASE:    // and RPi3 as is same value!
            s_ePiType = ARM7;
            break;
        case RPI4_BCM2708_PERI_BASE:
            s_ePiType = PI4;
            break;
        default:
            s_ePiType = NOTSET; // we just don't know!
            break;
    }

    if(s_ePiType != NOTSET) {
        // setup gpio memory map
        init_gpio_access();

        // setup timer memory map
        //init_timer_access();

        // setup interrupt table memory map
        init_interrupt_access();

        // initialize our pin-set
        initCurrentPins();

        // initialize our xmit bit table
        initBitTableForCurrentPins();
    }
}



static void init_gpio_access(void)
{
    void *gpio_map;

/*
    gpio = (volatile uint32_t *)GPIO_BASE;
    s_pGpioRegisters = (struct GpioRegisters *)(unsigned long)GPIO_BASE;
    printk(KERN_WARNING "LEDfifo: BCM2708_PERI_BASE=0x%8X\n", BCM2708_PERI_BASE);
    printk(KERN_WARNING "LEDfifo:         GPIO_BASE=0x%8X\n", GPIO_BASE);
    printk(KERN_WARNING "LEDfifo: GPIO Reg Bank  0x%p\n", (void *)s_pGpioRegisters);
    printk(KERN_WARNING "LEDfifo:      GPFSEL[0] 0x%p\n", &s_pGpioRegisters->GPFSEL[0]);
    printk(KERN_WARNING "LEDfifo:      GPFSEL[1] 0x%p\n", &s_pGpioRegisters->GPFSEL[1]);
    printk(KERN_WARNING "LEDfifo:      GPSET[0]  0x%p\n", &s_pGpioRegisters->GPSET[0]);
    printk(KERN_WARNING "LEDfifo:      GPSET[1]  0x%p\n", &s_pGpioRegisters->GPSET[1]);
    printk(KERN_WARNING "LEDfifo:      GPCLR[0]  0x%p\n", &s_pGpioRegisters->GPCLR[0]);
    printk(KERN_WARNING "LEDfifo:      GPCLR[1]  0x%p\n", &s_pGpioRegisters->GPCLR[1]);
    printk(KERN_WARNING "LEDfifo: GPIO_FSEL_REG0 0x%p\n", GPIO_FSEL_REG0);
    printk(KERN_WARNING "LEDfifo:  GPIO_SET_REG0 0x%p\n", GPIO_SET_REG0);
    printk(KERN_WARNING "LEDfifo:  GPIO_CLR_REG0 0x%p\n", GPIO_CLR_REG0);
*/
    uint32_t pGpioBaseAddress = s_pRPiModelIOBaseAddress + GPIO_OFFSET;
    gpio_map = ioremap(pGpioBaseAddress, GPIO_BLOCK_SIZE);
    printk(KERN_WARNING "LEDfifo:  GPIO MAP      0x%p\n", gpio_map);

    // Always use volatile pointer!
    gpio = (volatile uint32_t *)gpio_map;
    s_pGpioRegisters = (struct GpioRegisters *)gpio;
/*
    printk(KERN_WARNING "LEDfifo:  GPIO Reg Bank 0x%p\n", (void *)s_pGpioRegisters);
    printk(KERN_WARNING "LEDfifo:      GPFSEL[0] 0x%p\n", &(s_pGpioRegisters->GPFSEL[0]));
    printk(KERN_WARNING "LEDfifo:      GPFSEL[1] 0x%p\n", &(s_pGpioRegisters->GPFSEL[1]));
    printk(KERN_WARNING "LEDfifo:      GPSET[0]  0x%p\n", &(s_pGpioRegisters->GPSET[0]));
    printk(KERN_WARNING "LEDfifo:      GPSET[1]  0x%p\n", &s_pGpioRegisters->GPSET[1]);
    printk(KERN_WARNING "LEDfifo:      GPCLR[0]  0x%p\n", &s_pGpioRegisters->GPCLR[0]);
    printk(KERN_WARNING "LEDfifo:      GPCLR[1]  0x%p\n", &s_pGpioRegisters->GPCLR[1]);
    printk(KERN_WARNING "LEDfifo: GPIO_FSEL_REG0 0x%p\n", GPIO_FSEL_REG0);
    printk(KERN_WARNING "LEDfifo:  GPIO_SET_REG0 0x%p\n", GPIO_SET_REG0);
    printk(KERN_WARNING "LEDfifo:  GPIO_CLR_REG0 0x%p\n", GPIO_CLR_REG0);
    printk(KERN_WARNING "LEDfifo: GPIO_ADDR(GPREG_FSEL0)  0x%p\n", GPIO_ADDR(GPREG_FSEL0));
    printk(KERN_WARNING "LEDfifo: GPIO_ADDR(GPREG_SET0)  0x%p\n", GPIO_ADDR(GPREG_SET0));
    printk(KERN_WARNING "LEDfifo: GPIO_ADDR(GPREG_CLR0)  0x%p\n", GPIO_ADDR(GPREG_CLR0));
*/
}

volatile unsigned *s_pInterruptEnableBase;
volatile unsigned *s_pInterruptCoresEnableBase;

static void init_interrupt_access(void)
{
    uint32_t pIntrptBaseAddress = s_pRPiModelIOBaseAddress + INT_OFFSET;
    if(s_ePiType == PI4) {
        // we are RPi4!
        pIntrptBaseAddress = RPI4_GIC_distributor;
    }
    //int_map = mmap(NULL,4096,PROT_READ|PROT_WRITE, MAP_SHARED,memfd,baseadd+INT_BASE);
    s_pInterruptEnableBase = ioremap(pIntrptBaseAddress, INTR_BLOCK_SIZE);


    if(s_ePiType == ARM7) {
        // Pi2 or RPi3
        //quad_map = mmap(NULL,4096,PROT_READ|PROT_WRITE, MAP_SHARED,memfd,0x40000000);
        s_pInterruptCoresEnableBase = ioremap(RPI2_3_CORES_BASE, CORES_BLOCK_SIZE);
    }
}

volatile unsigned *s_pTimersBase;

static void init_timer_access(void)
{
    uint32_t pTimersBaseAddress = s_pRPiModelIOBaseAddress + TIMER_OFFSET;
    //timer_map = mmap(NULL,4096,PROT_READ|PROT_WRITE, MAP_SHARED,memfd,baseadd+TIMER_BASE);
    s_pTimersBase = ioremap(pTimersBaseAddress, TIMER_BLOCK_SIZE);
}

/******************** INTERRUPTS *************

  REF: https://www.raspberrypi.org/forums/viewtopic.php?f=29&t=52393&p=1559190&hilit=interrupts#p1559190

interrupts(0)   disable interrupts
interrupts(1)   re-enable interrupts

return 1 = OK
       0 = error with message print

Uses s_ePiType and s_pInterruptEnableBase/s_pInterruptCoresEnableBase pointers set by setup()

Avoid calling immediately after keyboard input
or key strokes will not be dealt with properly

*******************************************/

int interrupts(int bDisableRequest)
{
  int n;
  unsigned int temp131;
  static unsigned int sav132 = 0;
  static unsigned int sav133 = 0;
  static unsigned int sav134 = 0;
  static unsigned int sav4 = 0;
  static unsigned int sav16 = 0;
  static unsigned int sav17 = 0;
  static unsigned int sav18 = 0;
  static unsigned int sav19 = 0;
  static unsigned int sav20 = 0;
  static unsigned int sav21 = 0;
  static unsigned int sav22 = 0;
  static unsigned int sav23 = 0;
  static int s_bDisabledState = 0;


  if(s_ePiType == NOTSET) {
    printk(KERN_ERR "interrupts() Setup not done\n");
    return(0);
  }

  // disable
  if(bDisableRequest == 0) {
    if(s_bDisabledState != 0) {
      // Interrupts already disabled so avoid printf
      return(1);
    }

    // Note on register offsets
    // If a register is described in the documentation as offset 0x20C = 524 bytes
    // The pointers such as s_pInterruptEnableBase are 4 bytes long,
    // so s_pInterruptEnableBase+131 points to s_pInterruptEnableBase + 4x131 = offset 0x20C

    // save current interrupt settings

    if(s_ePiType == ARM7) {
      // Pi3 only
      sav4 = *(s_pInterruptCoresEnableBase+4);     // Performance Monitor Interrupts set  register 0x0010
      sav16 = *(s_pInterruptCoresEnableBase+16);   // Core0 timers Interrupt control  register 0x0040
      sav17 = *(s_pInterruptCoresEnableBase+17);   // Core1 timers Interrupt control  register 0x0044
      sav18 = *(s_pInterruptCoresEnableBase+18);   // Core2 timers Interrupt control  register 0x0048
      sav19 = *(s_pInterruptCoresEnableBase+19);   // Core3 timers Interrupt control  register 0x004C
      // the Mailbox interrupts are probably disabled anyway - but to be safe:
      sav20 = *(s_pInterruptCoresEnableBase+20);   // Core0 Mailbox Interrupt control  register 0x0050
      sav21 = *(s_pInterruptCoresEnableBase+21);   // Core1 Mailbox Interrupt control  register 0x0054
      sav22 = *(s_pInterruptCoresEnableBase+22);   // Core2 Mailbox Interrupt control  register 0x0058
      sav23 = *(s_pInterruptCoresEnableBase+23);   // Core3 Mailbox Interrupt control  register 0x005C
    }

    if(s_ePiType == PI4) {
      sav4 = *(s_pInterruptEnableBase);               // GICD_CTLR register
      *(s_pInterruptEnableBase) = sav4 & 0xFFFFFFFE;  // clear bit 0 enable forwarding to CPU
    }
    else {
      sav134 = *(s_pInterruptEnableBase+134);  // Enable basic IRQs register 0x218
      sav132 = *(s_pInterruptEnableBase+132);  // Enable IRQs 1 register 0x210
      sav133 = *(s_pInterruptEnableBase+133);  // Enable IRQs 2 register 0x214

      // Wait for pending interrupts to clear
      // Seems to work OK without this, but it does no harm
      // Limit to 100 tries to avoid infinite loop
      n = 0;
      while( (*(s_pInterruptEnableBase+128) | *(s_pInterruptEnableBase+129) | *(s_pInterruptEnableBase+130)) != 0 && n < 100) {
        ++n;
      }
    }

    // disable all interrupts

    if(s_ePiType == ARM7) {
      // Pi3 only
      *(s_pInterruptCoresEnableBase+5) = sav4;   // disable via Performance Monitor Interrupts clear  register 0x0014
      *(s_pInterruptCoresEnableBase+16) = 0;     // disable by direct write
      *(s_pInterruptCoresEnableBase+17) = 0;
      *(s_pInterruptCoresEnableBase+18) = 0;
      *(s_pInterruptCoresEnableBase+19) = 0;
      *(s_pInterruptCoresEnableBase+20) = 0;
      *(s_pInterruptCoresEnableBase+21) = 0;
      *(s_pInterruptCoresEnableBase+22) = 0;
      *(s_pInterruptCoresEnableBase+23) = 0;
    }

    if(s_ePiType != PI4) {
      temp131 = *(s_pInterruptEnableBase+131);  // read FIQ control register 0x20C
      temp131 &= ~(1 << 7);      // zero FIQ enable bit 7
      *(s_pInterruptEnableBase+131) = temp131;  // write back to register
                               // attempting to clear bit 7 of *(s_pInterruptEnableBase+131) directly
                               // will crash the system

      *(s_pInterruptEnableBase+135) = sav132; // disable by writing to Disable IRQs 1 register 0x21C
      *(s_pInterruptEnableBase+136) = sav133; // disable by writing to Disable IRQs 2 register 0x220
      *(s_pInterruptEnableBase+137) = sav134; // disable by writing to Disable basic IRQs register 0x224
    }

    s_bDisabledState = 1;   // interrupts disabled
  }
  else {
    // bDisableRequest = 1 enable
    if(s_bDisabledState == 0) {
      // Interrupts are enabled
      return(1);
    }

    // restore all saved interrupts
    if(s_ePiType == PI4) {
      *(s_pInterruptEnableBase) = sav4;  // set bit 0
    }
    else {
      *(s_pInterruptEnableBase+134) = sav134;
      *(s_pInterruptEnableBase+133) = sav133;
      *(s_pInterruptEnableBase+132) = sav132;

      temp131 = *(s_pInterruptEnableBase+131);      // read FIQ control register 0x20C
      temp131 |= (1 << 7);                          // set FIQ enable bit
      *(s_pInterruptEnableBase+131) = temp131;      // write back to register
    }

    if(s_ePiType == ARM7) {
      // Pi3 only
      *(s_pInterruptCoresEnableBase+4) = sav4;
      *(s_pInterruptCoresEnableBase+16) = sav16;
      *(s_pInterruptCoresEnableBase+17) = sav17;
      *(s_pInterruptCoresEnableBase+18) = sav18;
      *(s_pInterruptCoresEnableBase+19) = sav19;
      *(s_pInterruptCoresEnableBase+20) = sav20;
      *(s_pInterruptCoresEnableBase+21) = sav21;
      *(s_pInterruptCoresEnableBase+22) = sav22;
      *(s_pInterruptCoresEnableBase+23) = sav23;
    }

    s_bDisabledState = 0;   // indicates interrupts enabled (not disabled)
  }
  return(1);
}

// ---------------------
// Operation NOTE:
//  SetGPIOFunction( LedGpioPin, 0b001); // Output
//  SetGPIOFunction( LedGpioPin, 0); // Configure the pin as input
//
static void SetGPIOFunction(int GPIO, int functionCode)
{
    int registerIndex = GPIO / 10;
    int bit = (GPIO % 10) * 3;

    unsigned oldValue = s_pGpioRegisters-> GPFSEL[registerIndex];
    unsigned mask = 0b111 << bit;
    printk(KERN_INFO "LEDfifo: Changing function of GPIO%d from %x to %x\n",
           GPIO,
           (oldValue >> bit) & 0b111,
           functionCode);

    s_pGpioRegisters-> GPFSEL[registerIndex] =
        (oldValue & ~mask) | ((functionCode << bit) & mask);
}

/*
**  here as original example
**
static void SetGPIOOutputValue(int GPIO, bool outputValue)
{
    if (outputValue)
        s_pGpioRegisters->GPSET[GPIO / 32] = (1 << (GPIO % 32));
    else
        s_pGpioRegisters->GPCLR[GPIO / 32] = (1 << (GPIO % 32));
}
*/

static void resetCurrentPins(void)
{
    uint8_t nPinIndex;
    for(nPinIndex = 0; nPinIndex < FIFO_MAX_PIN_COUNT; nPinIndex++) {
        if(gpioPins[nPinIndex] != 0) {
            SetGPIOFunction(gpioPins[nPinIndex], 0b000);    // input
        }
    }
}

static void initCurrentPins(void)
{
    uint8_t nPinIndex;
    for(nPinIndex = 0; nPinIndex < FIFO_MAX_PIN_COUNT; nPinIndex++) {
        if(gpioPins[nPinIndex] != 0) {
            SetGPIOFunction(gpioPins[nPinIndex], 0b001);    // output
        }
    }
}



// ---------------------
// DURATION TABLE def's
//
enum eGpioOperationType {
    // zero NOT used, on purpose! (zero indicates value not set)
    OP_GPIO_SET=1,
    OP_GPIO_CLR=2
};

typedef struct _gpioCrontrolWord
{
    uint32_t gpioPinBits;   // 1 placed in each bit location
    uint16_t durationToNext;
    uint8_t gpioOperation;  // eGpioOperationType value SET/CLR
    uint8_t entryOccupied;
} gpioCrontrolWord_t;

#define MAX_GPIO_CONTROL_WORDS 3
#define MAX_GPIO_CONTROL_ENTRIES 8

typedef struct _gpioCrontrolEntry
{
    gpioCrontrolWord_t word[MAX_GPIO_CONTROL_WORDS];   // 1 placed in each bit location
} gpioCrontrolEntry_t;

static gpioCrontrolEntry_t gpioBitControlEntries[MAX_GPIO_CONTROL_ENTRIES];
static uint32_t pinsAllActive;

// ---------------------
// TABLE SETUP CODE
//

#define PIN_PRESENT(pinIdx) ((gpioPins[pinIdx] != 0) ? 1 : 0)
//#define PIN_VALUE_IF_SELECTED(index, ) ((gpioPins[pinIdx] != 0) ? 1 : 0)


static void initBitTableForCurrentPins(void)
{
    //
    //  our table lists on/off times for each bit pattern
    //   we have 8 table entries, one for each bit pattern:
    //        0b000, 0b001, 0b010, 0b011, 0b100, 0b101, 0b110, 0b111.
    //  bit masks are present in the table entry for each set and clear.
    //  table entries will be 1 set followed by 1 or 2 clears.
    //
    uint32_t pinValueIdx0;
    uint32_t pinValueIdx1;
    uint32_t pinValueIdx2;
    uint32_t pinsActiveHigh;
    uint32_t pinsActiveLow;
    uint8_t nTableIdx;
    uint8_t nWordIdx;
    uint8_t nPinCount;
    uint8_t nMaxTableEntries;
    uint8_t nMinHighPeriodLength;
    uint8_t nOnlyHighPeriodLength;
    uint8_t nOnlyRemainingPeriodLength;
    uint8_t nRemainingHighPeriodLength;
    uint8_t nRemainingLowPeriodLength;
    uint8_t n0IsShorterThan1;

    nPinCount  = (gpioPins[0] != 0) ? 1 : 0;
    nPinCount += (gpioPins[1] != 0) ? 1 : 0;
    nPinCount += (gpioPins[2] != 0) ? 1 : 0; // [0-3]

    switch(nPinCount) {
	case 3:
            nMaxTableEntries = 8;
            break;
	case 2:
            nMaxTableEntries = 4;
            break;
	case 1:
            nMaxTableEntries = 2;
            break;
	default:
            nMaxTableEntries = 0;
            break;
    }

#define CODE_LENGTH_IN_PERIODS 0
#define CODE_CORRECTION_LITERAL 0

    // zero fill our structure
    memset(gpioBitControlEntries, 0, sizeof(gpioBitControlEntries));

    // if we have table entries to populate...
    printk(KERN_INFO "LEDfifo: initBitTableForCurrentPins() loading %d entries\n", nMaxTableEntries);
    if(nMaxTableEntries > 0) {

        // set our pins
        pinValueIdx0 = (gpioPins[0] != 0) ? 1<<gpioPins[0] : 0;
        pinValueIdx1 = (gpioPins[1] != 0) ? 1<<gpioPins[1] : 0;
        pinValueIdx2 = (gpioPins[2] != 0) ? 1<<gpioPins[2] : 0;

        pinsAllActive = pinValueIdx0 | pinValueIdx1 | pinValueIdx2;

        n0IsShorterThan1 = (periodT0HCount < periodT1HCount);

        nMinHighPeriodLength = (n0IsShorterThan1) ? periodT0HCount : periodT1HCount;
        nRemainingHighPeriodLength = (n0IsShorterThan1) ? periodT1HCount - periodT0HCount : periodT0HCount - periodT1HCount;
        nRemainingLowPeriodLength = periodCount - (nMinHighPeriodLength + nRemainingHighPeriodLength + CODE_LENGTH_IN_PERIODS);

        // set bit on time
        nWordIdx = 0;
        for(nTableIdx = 0; nTableIdx < nMaxTableEntries; nTableIdx++) {
            if(nTableIdx == 0 || nTableIdx == nMaxTableEntries - 1) {
                // if we have all pins 0 or all pins 1 then...
                // do our only set (0 bits -or- 1 bits)
                nOnlyHighPeriodLength = (nTableIdx == 0) ? periodT0HCount : periodT1HCount;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].gpioPinBits = pinsAllActive;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].gpioOperation = OP_GPIO_SET;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].durationToNext = nOnlyHighPeriodLength * periodDurationNsec;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].entryOccupied = 1;

                // if we have all pins 0 or all pins 1 then...
                // do our only clear (0 bits -or- 1 bits)
                nOnlyRemainingPeriodLength = periodCount - nOnlyHighPeriodLength - CODE_LENGTH_IN_PERIODS;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].gpioPinBits = pinsAllActive;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].gpioOperation = OP_GPIO_CLR;
		// this is our last time value (reduce this amount to account for CPU time to next byte)
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].durationToNext = (nOnlyRemainingPeriodLength * periodDurationNsec) - CODE_CORRECTION_LITERAL;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].entryOccupied = 1;
            }
            else {
                // do our min-duration set for all active pins
                gpioBitControlEntries[nTableIdx].word[nWordIdx].gpioPinBits = pinsAllActive;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].gpioOperation = OP_GPIO_SET;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].durationToNext = nMinHighPeriodLength * periodDurationNsec;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].entryOccupied = 1;

                // calculate masks for early then late clears
                pinsActiveLow = ((nTableIdx & 0x01) == 0x01) ? 0 : pinValueIdx0;
                pinsActiveLow |= ((nTableIdx & 0x02) == 0x02) ? 0 : pinValueIdx1;
                pinsActiveLow |= ((nTableIdx & 0x04) == 0x04) ? 0 : pinValueIdx2;
                pinsActiveHigh = ((nTableIdx & 0x01) == 0x00) ? 0 : pinValueIdx0;
                pinsActiveHigh |= ((nTableIdx & 0x02) == 0x00) ? 0 : pinValueIdx1;
                pinsActiveHigh |= ((nTableIdx & 0x04) == 0x00) ? 0 : pinValueIdx2;
                // do our shorter clear (0or1 bits)
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].gpioPinBits = (n0IsShorterThan1) ? pinsActiveLow : pinsActiveHigh;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].gpioOperation = OP_GPIO_CLR;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].durationToNext = nRemainingHighPeriodLength * periodDurationNsec;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].entryOccupied = 1;
                // do our longer clear (1or0 bits)
                gpioBitControlEntries[nTableIdx].word[nWordIdx+2].gpioPinBits = (n0IsShorterThan1) ? pinsActiveHigh : pinsActiveLow;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+2].gpioOperation = OP_GPIO_CLR;
		// this is our last time value (reduce this amount to account for CPU time to next byte)
                gpioBitControlEntries[nTableIdx].word[nWordIdx+2].durationToNext = (nRemainingLowPeriodLength * periodDurationNsec) - CODE_CORRECTION_LITERAL;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+2].entryOccupied = 1;
            }
        }
    }
    dumpPinTable();
}

static void hexDump(const char message[], const char *addr, const int len) {
    int i;
    unsigned char buff[17];
    const unsigned char *pc = (const unsigned char*)addr;

    // Output description if given.
    if (message != NULL)
        printk(KERN_INFO "%s:\n", message);

    if (len == 0) {
        printk(KERN_INFO "  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printk(KERN_INFO "  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printk(KERN_INFO "  %s\n", buff);

            // Output the offset.
            printk(KERN_INFO "  %04x ", i);
        }

        // Now the hex code for the specific character.
        printk(KERN_INFO " %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printk(KERN_INFO "   ");
        i++;
    }

    // And print the final ASCII bit.
    printk(KERN_INFO "  %s\n", buff);
}

static void dumpPinTable(void)
{
    gpioCrontrolEntry_t *selectedEntry = NULL;
    gpioCrontrolWord_t *selectedWord = NULL;
    int nEntryIdx;
    int nWordIdx;
    char *opText;
    char *validText;

    printk(KERN_INFO "LEDfifo: dumpPinTable ------------------\n");

    for(nEntryIdx = 0; nEntryIdx < MAX_GPIO_CONTROL_ENTRIES; nEntryIdx++) {
        printk(KERN_INFO "LEDfifo: Entry for bits %x:\n", nEntryIdx);
        selectedEntry = &gpioBitControlEntries[nEntryIdx];
        for(nWordIdx = 0; nWordIdx < MAX_GPIO_CONTROL_WORDS; nWordIdx++) {
            selectedWord = &selectedEntry->word[nWordIdx];
            switch(selectedWord->gpioOperation) {
                case OP_GPIO_SET:
                    opText = "SET";
                    break;
                case OP_GPIO_CLR:
                    opText = "CLEAR";
                    break;
                default:
                    opText = "{not-set}";
                    break;
            }
            validText = (selectedWord->entryOccupied == 1) ? "YES" : "no";
            if(selectedWord->entryOccupied) {
                printk(KERN_INFO "LEDfifo:   - word %d -- bits %8X op:[%s] duration:%04d valid:%s\n", nWordIdx, selectedWord->gpioPinBits, opText, selectedWord->durationToNext, validText);
	    }
	    else {
                printk(KERN_INFO "LEDfifo:   - word %d -- empty --\n", nWordIdx);
            }
       }
    }

    printk(KERN_INFO "LEDfifo: dumpPinTable ------------------\n");
}

// debug counting of writes
#define MAX_COUNT_ENTRIES 8
static int nValueCountsAr[MAX_COUNT_ENTRIES];

static void clearCounts(void)
{
	int countIdx;
	for(countIdx = 0; countIdx<MAX_COUNT_ENTRIES; countIdx++) {
		nValueCountsAr[countIdx] = 0;
	}
}

static void showCounts(void)
{
	int countIdx;
    printk(KERN_INFO "LEDfifo: ----- bit-values sent----\n");
	for(countIdx = 0; countIdx<MAX_COUNT_ENTRIES; countIdx++) {
        	printk(KERN_INFO "LEDfifo: value(0x%02X) %d x\n", countIdx, nValueCountsAr[countIdx]);
	}
    printk(KERN_INFO "LEDfifo: -------------------------\n");
}

// ============================================================================
// ---------------------
// GPIO execution code
//   NOTE: RESTRICTION: All GPIO pins are in 0-31 range!
//
//

static void xmitBitValuesToAllChannels(uint8_t bitsIndex)
{
    gpioCrontrolEntry_t *selectedEntry = NULL;
    gpioCrontrolWord_t *selectedWord = NULL;
    uint8_t nWordIdx;
    // spinlock_t mr_lock = SPIN_LOCK_UNLOCKED;
    // DEFINE_SPINLOCK(mr_lock);
    // unsigned long flags;

    //printk(KERN_INFO "LEDfifo: xmitBitValuesToAllChannels(%d)\n", bitsIndex);
    if(bitsIndex >= MAX_GPIO_CONTROL_ENTRIES) {
        printk(KERN_ERR "LEDfifo: [CODE] xmitBitValuesToAllChannels(%d) OUT-OF-RANGE bitIndex not [0-%d]\n", bitsIndex, MAX_GPIO_CONTROL_ENTRIES-1);
    }
    else {

	// ============= BEGIN CRITICAL SECTION ==================
	//
	// let's prevent interrupts for this one LED write
	//spin_lock_irqsave(&mr_lock, flags);
	// spin_lock_irq(&mr_lock); // shorter version?

        nValueCountsAr[bitsIndex]++;	// count this send
        // select a table entry
        // then do timed set and clear(s) based on entry content
        selectedEntry = &gpioBitControlEntries[bitsIndex];

        for(nWordIdx = 0; nWordIdx < MAX_GPIO_CONTROL_WORDS; nWordIdx++) {
            selectedWord = &selectedEntry->word[nWordIdx];
            // is this entry valid?
            if(selectedWord->entryOccupied) {
                // yes, valid, do what it says...
                if(selectedWord->gpioOperation == OP_GPIO_SET) {
                    s_pGpioRegisters->GPSET[0] = selectedWord->gpioPinBits;
                }
                else if(selectedWord->gpioOperation == OP_GPIO_CLR) {
                    s_pGpioRegisters->GPCLR[0] = selectedWord->gpioPinBits;
               }
                else {
                    printk(KERN_ERR "LEDfifo: [CODE] xmitBitValuesToAllChannels(%d) INVALID gpioOperation Entry (%d) word[%d]\n", bitsIndex, selectedWord->gpioOperation, nWordIdx);
                }
            }
            // lessee if RPi has working ndelay()...
            //ndelay(selectedWord->durationToNext * periodDurationNsec / 2);
            // yeah, no.  Let's use ours...
            nSecDelay(selectedWord->durationToNext);
        }
	// and then allow interrupts once again...
	//spin_unlock_irqrestore(&mr_lock, flags);
	// spin_unlock_irq(&mr_lock); // shorter version?
	//
	// ============== END CRITICAL SECTION ===================
    }
}

static void xmitResetToAllChannels(void)
{
    printk(KERN_INFO "LEDfifo: xmitResetToAllChannels()\n");
    s_pGpioRegisters->GPCLR[0] = pinsAllActive;
    // lessee if RPi has working ndelay()...
    ndelay((periodTRESETCount * periodDurationNsec) / 2);
}


// ============================================================================
//  our tasklet: write 0's or 1's to our gpio ports for analysis with oscilloscope
//    sceduled with: tasklet_schedule(&my_tasklet);    /* mark my_tasklet as pending */
//    sceduled with: tasklet_hi_schedule(&my_tasklet);    /* mark my_tasklet as pending but run HI Priority */
//
void taskletTestWrites(unsigned long data)
{
    DEFINE_SPINLOCK(mr_lock);
    unsigned long flags;

    printk(KERN_INFO "LEDfifo: taskletTestWrites(%ld) ENTRY\n", data);

	// ============= BEGIN CRITICAL SECTION ==================
	//
	// let's prevent interrupts for this burst of LED writes
	spin_lock_irqsave(&mr_lock, flags);
	interrupts(0);   // disable

    // data is [0,1] for directing write of 0's or 1's test pattern
    if(data == 0) {
        testXmitZeros(1008);	// 1008 is 24 bits * 42 (42 LEDs)
    }
    else {
        testXmitOnes(1008);
    }

	// and then allow interrupts once again...
	interrupts(1);   // re-enable
	spin_unlock_irqrestore(&mr_lock, flags);
	//
	// ============== END CRITICAL SECTION ===================

    printk(KERN_INFO "LEDfifo: taskletTestWrites() EXIT\n");
}


static void testXmitZeros(uint32_t nCount)
{
    int nCounter;

    printk(KERN_INFO "LEDfifo: testXmitZeros(x %d)\n", nCount);
    if(nCount > 0) {
        for(nCounter = 0; nCounter < nCount; nCounter++) {
            xmitBitValuesToAllChannels(0b000);
        }
    }
}


static void testXmitOnes(uint32_t nCount)
{
    int nCounter;

    printk(KERN_INFO "LEDfifo: testXmitOnes(x %d)\n", nCount);
    if(nCount > 0) {
        for(nCounter = 0; nCounter < nCount; nCounter++) {
            xmitBitValuesToAllChannels(0b111);
        }
    }
}

#define TEST_GPIO_PIN 17

static void testXmitBit(uint16_t onDelay, uint16_t offDelay)
{
	static int s_bFirstTime = 0;

	if(s_bFirstTime == 0) {
        s_bFirstTime++;
        printk(KERN_INFO "LEDfifo: testXmitBiti(on %d nSec, off %d nSec)\n", onDelay, offDelay);
    }

	s_pGpioRegisters->GPSET[0] = 1 << TEST_GPIO_PIN;
 	nSecDelay(onDelay);

	s_pGpioRegisters->GPCLR[0] = 1 << TEST_GPIO_PIN;
 	nSecDelay(offDelay);
}

//  x8 = <<3 + x2 = <<1 -> x10!
#define SHIFT_MULT_X10(value) ((value << 3) + (value << 1))
//  x64 = <<7 + x32 = <<6 + x4 = << 2 -> x100!
#define SHIFT_MULT_X100(value) ((value << 6) + (value << 5) + (value << 2))

void nSecDelay(int nSecDuration)
{
    volatile int ctr;
    volatile int tst;

    //int delayCount = SHIFT_MULT_X10(nSecDuration) / 165; // div by 16.5 worse
    //int delayCount = SHIFT_MULT_X10(nSecDuration) / 170; // div by 17.0
    //int delayCount = SHIFT_MULT_X10(nSecDuration) / 173; // div by 17.3 worse
    //int delayCount = SHIFT_MULT_X10(nSecDuration) / 174; // div by 17.4 best
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1742; // div by 17.42
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1745; // div by 17.45
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1748; // div by 17.48
    //int delayCount = SHIFT_MULT_X10(nSecDuration) / 175; // div by 17.5 better
    //int delayCount = SHIFT_MULT_X10(nSecDuration) / 178; // div by 17.8 worse


    //int delayCount = SHIFT_MULT_X10(nSecDuration) / 174; // div by 17.4 best (T0H 370)
    //int delayCount = SHIFT_MULT_X10(nSecDuration) / 184; // div by 18.4 best (T0H 350)
    //int delayCount = SHIFT_MULT_X10(nSecDuration) / 163; // div by 16.3 best (T0H 400-420)
    //int delayCount = SHIFT_MULT_X10(nSecDuration) / 164; // div by 16.4 best (T0H 375-385)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1635; // div by 16.35 best (T0H 375-385)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1625; // div by 16.35 best (T0H 375-385)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1615; // div by 16.35 best (T0H 395-400)

    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1615; // div by 16.35 best (T1H 860-870)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1625; // div by 16.35 best (T0H 375-385)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1627; // div by 16.35 best (T0H 380-385)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1637; // div by 16.37 best (T0H 390-405)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1636; // div by 16.37 best (T0H 375-385)

    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1637; // div by 16.37 best (T0H 365-370: want 392)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1627; // div by 16.37 best (T0H 365-370: want 392)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1227; // div by 16.37 best (T0H 365-370: want 392)

    // AT cpuinfo_cur_freq = 1,500,000 (vs. 600,000)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1627; // div by 16.37 best (T0H 150) @1.5 GHz clock
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1227; // div by 16.37 best (T0H 210-220)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 30; // div by 00.30 best (T0H 7800)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 4030; // div by 40.30 best (T0H 60)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1030; // div by 10.30 best (T0H 250-260)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 1100; // div by 11.00 best (T0H 240)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 950; // div by 9.50 best (T0H 280-300)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 750; // div by 7.50 best (T0H 340-350)
    //int delayCount = SHIFT_MULT_X100(nSecDuration) / 600; // div by 6.00 best (T0H 430-440)
    int delayCount = SHIFT_MULT_X100(nSecDuration) / 656; // div by 6.00 best (T0H 380-390)

    for(ctr=0; ctr<delayCount; ctr++) { tst++; }
}



//  our tasklet: write single color to entire LED Matrix
//
void taskletScreenFill(unsigned long data)
{
    // data is 24-bit RGB value to be written
    uint8_t buffer[3];      // our 3 isolated colors
    uint8_t nPanelByte[3];	// 1 byte buffer for each panel (not 256x3 bytes)

    uint16_t nBytesWritten;
    uint16_t nLedIdx;

    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t nColorIdx;  // [0-2]
    uint8_t nPanelIdx;  // [0-2]
    uint8_t nBitShiftCount;  // [0-7]
    uint8_t nAllBits;

    DEFINE_SPINLOCK(mr_lock);
    unsigned long flags;

    clearCounts();
    nBytesWritten = 0;


    printk(KERN_INFO "LEDfifo: taskletScreenFill(0x%08lX) ENTRY\n", data);

    red = (data >> 16) & 0x000000ff;
    green = (data >> 8) & 0x000000ff;
    blue = (data >> 0) & 0x000000ff;

    // in memory the colors for the LED String are ordered as GRB!!!!
    // for this form, taskletScreenFill(), we simply have a 3-byte buffer we use for all LEDs on all Panels

    buffer[0] = green;
    buffer[1] = red;
    buffer[2] = blue;

	// ============= BEGIN CRITICAL SECTION ==================
	//
	// let's prevent interrupts for this burst of LED writes
	spin_lock_irqsave(&mr_lock, flags);
	interrupts(0);   // disable

   // for each LED in a panel
    for(nLedIdx = 0; nLedIdx < HARDWARE_MAX_LEDS_PER_PANEL; nLedIdx++) {
        // for each COLOR of an LED (24 bit, 3 bytes)
        for(nColorIdx = 0; nColorIdx < HARDWARE_MAX_COLOR_BYTES_PER_LED; nColorIdx++) {
            // grab single byte of color and set it for all panels
            nPanelByte[0] = buffer[nColorIdx];
            nPanelByte[1] = buffer[nColorIdx];
            nPanelByte[2] = buffer[nColorIdx];

            // for ea. bit MSBit to LSBit... [OR-in each of the three panel bits 0b00000321] then write all 3 gpio pins
            for(nBitShiftCount = 0; nBitShiftCount < 8; nBitShiftCount++) {
                // mask out the bits and OR them together (so they can all be written at one time)
                nAllBits = 0;
                for(nPanelIdx = 0; nPanelIdx < HARDWARE_MAX_PANELS; nPanelIdx++) {
                    nAllBits |= ((nPanelByte[nPanelIdx] >> (7 - nBitShiftCount)) & 0x01) << nPanelIdx;
                }
                xmitBitValuesToAllChannels(nAllBits);
            }

    	    // count this a 1 byte written - will be three per LED!
            nBytesWritten++;
        }
    }

	interrupts(1);   // re-enable
	// and then allow interrupts once again...
	spin_unlock_irqrestore(&mr_lock, flags);
	//
	// ============== END CRITICAL SECTION ===================

    xmitResetToAllChannels();

    printk(KERN_INFO "LEDfifo: -------------------------\n");
    printk(KERN_INFO "LEDfifo: %d bytes written\n", nBytesWritten);
    showCounts();
    printk(KERN_INFO "LEDfifo: taskletScreenFill() EXIT\n");
}

void taskletScreenWrite(unsigned long data)
{
    // no data?!  just write our single buffer to screen via GPIO?
    // data is 24-bit RGB value to be written
    uint8_t *pPanelByte[3];
    uint16_t nPanelOffsetInBytes[3];
    uint16_t nBytesWritten;


    uint16_t nLedIdx;   // [0-255]
    uint16_t nLEDOffset;  // [0-765]
    uint8_t nColorOffset;  // [0-2]

    uint8_t nPanelIdx;  // [0-2]
    uint8_t nBitShiftCount;  // [0-7]
    uint8_t nAllBits;

    DEFINE_SPINLOCK(mr_wr_lock);
    unsigned long flags;

    clearCounts();
    nBytesWritten = 0;

    printk(KERN_INFO "LEDfifo: taskletScreenWrite(0x%p) ENTRY\n", (void *)data);

    // in memory the colors for the LED String are ordered as GRB!!!!
    // for this form, taskletScreenWrite(), we translate the current contents of kernel_buffer writing results to our GPIO's

    nPanelOffsetInBytes[0] = 0;
    nPanelOffsetInBytes[1] = 1 * (HARDWARE_MAX_LEDS_PER_PANEL * HARDWARE_MAX_COLOR_BYTES_PER_LED);
    nPanelOffsetInBytes[2] = 2 * (HARDWARE_MAX_LEDS_PER_PANEL * HARDWARE_MAX_COLOR_BYTES_PER_LED);

	// ============= BEGIN CRITICAL SECTION ==================
	//
	// let's prevent interrupts for this burst of LED writes
	spin_lock_irqsave(&mr_wr_lock, flags);
	interrupts(0);   // disable

   // for each LED in a panel
    for(nLedIdx = 0; nLedIdx < HARDWARE_MAX_LEDS_PER_PANEL; nLedIdx++) {
        // calc offset to LED 3-bytes within panel
        nLEDOffset = (nLedIdx * HARDWARE_MAX_COLOR_BYTES_PER_LED);
        // for each COLOR of an LED (24 bit, 3 bytes)
        for(nColorOffset = 0; nColorOffset < HARDWARE_MAX_COLOR_BYTES_PER_LED; nColorOffset++) {
            // set pointer to next byte for each of our three panels
            for(nPanelIdx = 0; nPanelIdx < HARDWARE_MAX_PANELS; nPanelIdx++) {
                pPanelByte[nPanelIdx] = &kernel_buffer[nPanelOffsetInBytes[nPanelIdx] + nLEDOffset + nColorOffset];
            }

            // for ea. bit MSBit to LSBit... [OR-in each of the three panel bits 0b00000321] then write all 3 gpio pins
            for(nBitShiftCount = 0; nBitShiftCount < 8; nBitShiftCount++) {
                // mask out the bits and OR them together (so they can all be written at one time)
                nAllBits = 0;
                for(nPanelIdx = 0; nPanelIdx < HARDWARE_MAX_PANELS; nPanelIdx++) {
                    nAllBits |= ((*pPanelByte[nPanelIdx] >> (7 - nBitShiftCount)) & 0x01) << nPanelIdx;
                }
                xmitBitValuesToAllChannels(nAllBits);
            }

    	    // count this a 1 byte written - will be three per LED!
            nBytesWritten++;
        }
    }

	// and then allow interrupts once again...
	interrupts(1);   // re-enable
	spin_unlock_irqrestore(&mr_wr_lock, flags);
	//
	// ============== END CRITICAL SECTION ===================

    xmitResetToAllChannels();

    printk(KERN_INFO "LEDfifo: -------------------------\n");
    printk(KERN_INFO "LEDfifo: %d bytes written\n", nBytesWritten);
    showCounts();
    printk(KERN_INFO "LEDfifo: taskletScreenWrite() EXIT\n");

}

