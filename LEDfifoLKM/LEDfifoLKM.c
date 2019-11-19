/**
 * @file    LEDfifoLKM.c
 * @author  Stephen M Moraco
 * @date    15 November 2019
 * @version 0.1
 * @brief  An introductory "Hello World!" loadable kernel module (LKM) that can display a message
 * in the /var/log/kern.log file when the module is loaded and removed. The module can accept an
 * argument when it is loaded -- the name, which appears in the kernel log files.
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.
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

// get raspbery PI details
#include <asm/io.h>
//#include <mach/platform.h>
#include <linux/delay.h>
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
#define DEFAULT_PERIOD_IN_NSEC 50
#define DEFAULT_PERIOD_COUNT 25
#define DEFAULT_T0H_COUNT 8
#define DEFAULT_T1H_COUNT 16
#define DEFAULT_TRESET_COUNT 1000
#define DEFAULT_LOOP_ENABLE 0

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
#define STR_PRINTF_RET(len, str, args...) len += sprintf(page + len, str, ## args)
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0))
#define STR_PRINTF_RET(len, str, args...) len += seq_printf(m, str, ## args)
#else
#define STR_PRINTF_RET(len, str, args...) seq_printf(m, str, ## args)
#endif

// forward declarations
static long LEDfifo_ioctl(struct file *f, unsigned int cmd, unsigned long arg);
static void resetCurrentPins(void);
static void initCurrentPins(void);
static void initBitTableForCurrentPins(void);
static void transmitToAllChannelsBitsValued(uint8_t bitsIndex);
static void textXmitZeros(uint32_t nCount);
static void textXmitOnes(uint32_t nCount);
static void actOnCommand(void);

void taskletTestWrites(unsigned long data);
void taskletScreenFill(unsigned long data);
void taskletScreenWrite(unsigned long data);


// ----------------------------------------------------------------------------
//  SECTION: File-scoped Global Variables
//
static dev_t firstDevNbr; // Global variable for the first device number
static struct cdev c_dev; // Global variable for the character device structure
static struct class *cl; // Global variable for the device class

static struct proc_dir_entry *parent;
static struct proc_dir_entry *file;

static unsigned char ledType[FIFO_MAX_STR_LEN+1] = DEFAULT_LED_STRTYPE; // +1 for zero term.
static int gpioPins[FIFO_MAX_PIN_COUNT];    // max 3 gpio pins can be assigned
static int periodDurationNsec = DEFAULT_PERIOD_IN_NSEC;
static int periodCount = DEFAULT_PERIOD_COUNT;
static int periodT0HCount = DEFAULT_T0H_COUNT;
static int periodT1HCount = DEFAULT_T1H_COUNT;
static int periodTRESETCount = DEFAULT_TRESET_COUNT;
static int loopEnabled = DEFAULT_LOOP_ENABLE;

enum _DriverCommands {
    NO_CMD = 0,
    DO_FILL_SCREEN,
};
static int requestedCommand;
static int requestedColor;

static struct tasklet_struct tasklet;

// ----------------------------------------------------------------------------
//  SECTION: file-I/O handlers
//
static int LEDfifo_open(struct inode *i, struct file *f)
{
    printk(KERN_INFO "Driver: open()\n");
    return 0;
}


static int LEDfifo_close(struct inode *i, struct file *f)
{
    printk(KERN_INFO "Driver: close()\n");
    return 0;
}


static ssize_t LEDfifo_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    printk(KERN_INFO "Driver: read()\n");
    return 0;
}

static ssize_t LEDfifo_readv(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    printk(KERN_INFO "Driver: readv()\n");
    return 0;
}


static ssize_t LEDfifo_write(struct file *f, const char __user *buf, size_t len,
    loff_t *off)
{
    printk(KERN_INFO "Driver: write()\n");
    return len;
}

static ssize_t LEDfifo_writev(struct file *f, const char __user *buf, size_t len,
    loff_t *off)
{
    printk(KERN_INFO "Driver: writev()\n");
    return len;
}

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

    printk(KERN_INFO "Driver: ioctl()\n");

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
            break;
        case CMD_RESET_VARIABLES:
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
            loopEnabled = arg;
            break;
        case CMD_GET_LOOP_ENABLE:
            retval = loopEnabled;
            break;
        case CMD_TEST_BIT_WRITES:
            if(arg == 0) {
                //textXmitZeros(1000);
                tasklet_init(&tasklet, taskletTestWrites, 0); 
                tasklet_hi_schedule(&tasklet);
            }
            else {
                //textXmitOnes(1000);
                tasklet_init(&tasklet, taskletTestWrites, 1); 
                tasklet_hi_schedule(&tasklet);
           }
            break;
        case CMD_CLEAR_SCREEN:
            requestedCommand = DO_FILL_SCREEN;
            requestedColor = 0x000000;  // black
            tasklet_init(&tasklet, taskletScreenFill, requestedColor); 
            tasklet_hi_schedule(&tasklet);
            break;
        case CMD_SET_SCREEN_COLOR:
            requestedCommand = DO_FILL_SCREEN;
            requestedColor = arg;
            tasklet_init(&tasklet, taskletScreenFill, requestedColor); 
            tasklet_hi_schedule(&tasklet);
            break;
        default:
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
    //freqInKHz = 1 / (periodDurationNsec * periodCount * 0.000000001);
    STR_PRINTF_RET(len, "Serial Stream: %d nSec Period (%d x %d nSec increments)\n", (periodCount * periodDurationNsec), periodCount,  periodDurationNsec);
    //STR_PRINTF_RET(len, "Serial Stream: %d KHz (%d x %d nSec periods)\n", freqInKHz, periodCount,  periodDurationNsec);
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
        //printk(KERN_WARNING "LEDfifo: can't get major %d\n", scull_major);
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
    
    // initialize our pin-set
    initCurrentPins();
    // initialize our xmit bit table
    initBitTableForCurrentPins();
    
    printk(KERN_INFO "LEDfifo: init EXIT\n");

    return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit LEDfifoLKM_exit(void){
    printk(KERN_INFO "LEDfifo: Exit(%s)\n", name);
    
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
};
 
//struct GpioRegisters *s_pGpioRegisters = (struct GpioRegisters *)__io_address(GPIO_BASE);

#define BCM2708_PERI_BASE        0xFE000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */

struct GpioRegisters *s_pGpioRegisters = (struct GpioRegisters *)GPIO_BASE;

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
    printk(KERN_INFO "Changing function of GPIO%d from %x to %x\n", 
           GPIO,
           (oldValue >> bit) & 0b111,
           functionCode);
 
    s_pGpioRegisters-> GPFSEL[registerIndex] = 
        (oldValue & ~mask) | ((functionCode << bit) & mask);
}
 
static void SetGPIOOutputValue(int GPIO, bool outputValue)
{
    if (outputValue)
        s_pGpioRegisters->GPSET[GPIO / 32] = (1 << (GPIO % 32));
    else
        s_pGpioRegisters->GPCLR[GPIO / 32] = (1 << (GPIO % 32));
}

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
    GPIO_SET=1, 
    GPIO_CLR=2 
};

typedef struct _gpioCrontrolWord
{
    uint32_t gpioPinBits;   // 1 placed in each bit location
    uint8_t gpioOperation;  // eGpioOperationType value SET/CLR
    uint8_t durationToNext;
    uint8_t entryOccupied;
    uint8_t unused1;
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
    uint32_t pinsAllHigh;
    uint32_t pinsAllLow;
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
   
    nPinCount = (gpioPins[0] != 0) ? 1 : 0 + 
                (gpioPins[1] != 0) ? 1 : 0 + 
                (gpioPins[2] != 0) ? 1 : 0; // [0-3]
                    
    nMaxTableEntries = (nPinCount > 2) ? 8 : (nPinCount > 1) ? 4 : (nPinCount > 0) ? 2 : 0;
                        
    // zero fill our structure
    memset(gpioBitControlEntries, 0, sizeof(gpioBitControlEntries));
    
    // if we have table entries to populate...
    if(nMaxTableEntries > 0) {
        
        // set our pins
        pinValueIdx0 = (gpioPins[0] != 0) ? 1<<gpioPins[0] : 0;
        pinValueIdx1 = (gpioPins[1] != 0) ? 1<<gpioPins[1] : 0;
        pinValueIdx2 = (gpioPins[2] != 0) ? 1<<gpioPins[2] : 0;
            
        pinsAllActive = pinValueIdx0 | pinValueIdx1 | pinValueIdx2;
        
        n0IsShorterThan1 = (periodT0HCount > periodT1HCount);
            
        nMinHighPeriodLength = (n0IsShorterThan1) ? periodT0HCount : periodT1HCount;
        nRemainingHighPeriodLength = (n0IsShorterThan1) ? periodT1HCount - periodT0HCount : periodT0HCount - periodT1HCount;
        nRemainingLowPeriodLength = periodCount - (nMinHighPeriodLength + nRemainingHighPeriodLength);
            
        // set bit on time
        nWordIdx = 0;
        for(nTableIdx = 0; nTableIdx < nMaxTableEntries; nTableIdx++) {
            if(nTableIdx == 0 || nTableIdx == nMaxTableEntries - 1) {
                // if we have all pins 0 or all pins 1 then...
                // do our only set (0 bits -or- 1 bits)
                nOnlyHighPeriodLength = (nTableIdx == 0) ? periodT0HCount : periodT1HCount;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].gpioPinBits = pinsAllActive;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].gpioOperation = GPIO_SET;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].durationToNext = nOnlyHighPeriodLength;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].entryOccupied = 1;
                
                // if we have all pins 0 or all pins 1 then...
                // do our only clear (0 bits -or- 1 bits)
                nOnlyRemainingPeriodLength = periodCount - nOnlyHighPeriodLength;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].gpioPinBits = pinsAllActive;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].gpioOperation = GPIO_CLR;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].durationToNext = nOnlyRemainingPeriodLength;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].entryOccupied = 1;
            }
            else {
                // do our min-duration set for all active pins               
                gpioBitControlEntries[nTableIdx].word[nWordIdx].gpioPinBits = pinsAllActive;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].gpioOperation = GPIO_SET;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].durationToNext = nMinHighPeriodLength;
                gpioBitControlEntries[nTableIdx].word[nWordIdx].entryOccupied = 1;
                
                // calculate masks for early then late clears
                pinsAllLow = ((nTableIdx & 0x01) == 0x01) ? 0 : pinValueIdx0;
                pinsAllLow |= ((nTableIdx & 0x02) == 0x02) ? 0 : pinValueIdx1;
                pinsAllLow |= ((nTableIdx & 0x04) == 0x04) ? 0 : pinValueIdx2;
                pinsAllHigh = ((nTableIdx & 0x01) == 0x01) ? 0 : pinValueIdx0;
                pinsAllHigh |= ((nTableIdx & 0x02) == 0x02) ? 0 : pinValueIdx1;
                pinsAllHigh |= ((nTableIdx & 0x04) == 0x04) ? 0 : pinValueIdx2;
                // do our shorter clear (0or1 bits)
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].gpioPinBits = (n0IsShorterThan1) ? pinsAllLow : pinsAllHigh;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].gpioOperation = GPIO_CLR;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].durationToNext = nRemainingHighPeriodLength;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+1].entryOccupied = 1;
                // do our longer clear (1or0 bits)
                gpioBitControlEntries[nTableIdx].word[nWordIdx+2].gpioPinBits = (n0IsShorterThan1) ? pinsAllHigh : pinsAllLow;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+2].gpioOperation = GPIO_CLR;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+2].durationToNext = nRemainingLowPeriodLength;
                gpioBitControlEntries[nTableIdx].word[nWordIdx+2].entryOccupied = 1;
            }
        }
    }
}

// ---------------------
// GPIO execution code
//   NOTE: RESTRICTION: All GPIO pins are in 0-31 range!
//
//

static void transmitToAllChannelsBitsValued(uint8_t bitsIndex)
{
    gpioCrontrolEntry_t *selectedEntry = NULL;
    gpioCrontrolWord_t *selectedWord = NULL;
    uint8_t nWordIdx;
    
    if(bitsIndex >= MAX_GPIO_CONTROL_ENTRIES) {
        printk(KERN_ERR "LEDfifo: [CODE] transmitToAllChannelsBitsValued(%d) OUT-OF-RANGE bitIndex not [0-%d]\n", bitsIndex, MAX_GPIO_CONTROL_ENTRIES-1);
    }
    else {
        // select a table entry
        // then do timed set and clear(s) based on entry content
        selectedEntry = &gpioBitControlEntries[bitsIndex];
        
        for(nWordIdx = 0; nWordIdx < MAX_GPIO_CONTROL_WORDS; nWordIdx++) {
            selectedWord = selectedEntry->word[nWordIdx];
            // is this entry valid?
            if(selectedWord->entryOccupied) {
                // yes, valid, do what it says...
                if(selectedWord->gpioOperation == GPIO_SET) {
                    s_pGpioRegisters->GPSET[0] = selectedEntry->gpioPinBits;
                }
                else if(selectedWord->gpioOperation == GPIO_CLR) {
                    s_pGpioRegisters->GPCLR[0] = selectedWord->gpioPinBits;
               }
                else {
                    printk(KERN_ERR "LEDfifo: [CODE] transmitToAllChannelsBitsValued(%d) INVALID gpioOperation Entry (%d) word[%d]\n", bitsIndex, selectedEntry->gpioOperation, nWordIdx);
                }
            }
            // lessee if RPi has working ndelay()...
            ndelay(selectedWord->durationToNext * periodDurationNsec);
        }
    }
}

static void transmitResetAllChannelsBits(void)
{
    s_pGpioRegisters->GPCLR[0] = pinsAllActive;
    // lessee if RPi has working ndelay()...
    ndelay(periodTRESETCount * periodDurationNsec);
}

static void actOnCommand(void)
{
    // act on globals telling us what needs to be done
    // globals:
    //   requestedCommand - what do do
    //   requestedColor - useful if command requires color
    //
    switch(requestedCommand) {
        case DO_FILL_SCREEN:
            // fill screen with requestedColor value (RGB)
            break;
        case NO_CMD:
            break;
        default:
            break;
    }
}

static void textXmitZeros(uint32_t nCount)
{
    int nCounter;
    if(nCount > 0) {
        for(nCounter = 0; nCounter < nCount; nCounter++) {
            transmitToAllChannelsBitsValued(0b000);
        }
    }
}


static void textXmitOnes(uint32_t nCount)
{
    int nCounter;
    if(nCount > 0) {
        for(nCounter = 0; nCounter < nCount; nCounter++) {
            transmitToAllChannelsBitsValued(0b111);
        }
    }
}

//
//  our tasklet: write values to LED Matrix
//    sceduled with: tasklet_schedule(&my_tasklet);    /* mark my_tasklet as pending */
//    sceduled with: tasklet_hi_schedule(&my_tasklet);    /* mark my_tasklet as pending but run HI Priority */
//
void taskletTestWrites(unsigned long data)
{
    // data is [0,1] for directing write of 0's or 1's test pattern
    if(data == 0) {
        textXmitZeros(1000);
    }
    else {
        textXmitOnes(1000);
    }
}

void taskletScreenFill(unsigned long data)
{
    // data is 24-bit RGB value to be written
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t buffer[3];
    uint8_t nLedIdx;
    uint8_t nColorIdx;
    uint8_t nPanelIdx;
    uint8_t nBitShiftValue;
    uint8_t pPanelByte[3];
   
    
    red = (data >> 16) & 0x000000ff;
    green = (data >> 8) & 0x000000ff;
    blue = (data >> 0) & 0x000000ff;
    buffer[0] = green;
    buffer[1] = red;
    buffer[2] = blue;
    
    for(nLedIdx = 0; nLedIdx < 256; nLedIdx++) {
        for(nColorIdx = 0; nColorIdx < 3; nColorIdx++) {
            pPanelByte[0] = &buffer[nColorIdx];
            pPanelByte[1] = &buffer[nColorIdx];
            pPanelByte[2] = &buffer[nColorIdx];
            for(nBitShiftValue = 7; nBitShiftValue >=0; nBitShiftValue--) {
                nBitShiftValue = 0;
                for(nPanelIdx = 0; nPanelIdx < 3; nPanelIdx++) {
                    nBitShiftValue |= ((pPanelByte[nPanelIdx] >> nBitShiftValue) & 0x01) << nPanelIdx;
                }
                transmitToAllChannelsBitsValued(nBitShiftValue);
            }
        }
    }
    transmitResetAllChannelsBits();
}

void taskletScreenWrite(unsigned long data)
{
    // no data?  just write our single buffer to screen via GPIO?
}
 
