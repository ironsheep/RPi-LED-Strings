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

#include "LEDfifoConfigureIOCtl.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
#define STR_PRINTF_RET(len, str, args...) len += sprintf(page + len, str, ## args)
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0))
#define STR_PRINTF_RET(len, str, args...) len += seq_printf(m, str, ## args)
#else
#define STR_PRINTF_RET(len, str, args...) seq_printf(m, str, ## args)
#endif

MODULE_LICENSE("GPL");              ///< The license type -- this affects runtime behavior
MODULE_AUTHOR("Stephen M Moraco");      ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("An LED Matrix display GPIO Driver.");  ///< The description -- see modinfo
MODULE_VERSION("0.1");              ///< The version of the module
 
static char *name = "{nameParm}";        ///< An example LKM argument -- default value is "{nameParm}"
module_param(name, charp, S_IRUGO); ///< Param desc. charp = char ptr, S_IRUGO can be read/not changed
MODULE_PARM_DESC(name, "The name to display in /var/log/kern.log");  ///< parameter description
 
#define LED_FIFO_MAJOR 0   /* dynamic major by default */
#define LED_FIFO_NR_DEVS 1    /* ledfifo0  (not ledfifo0-ledfifoN) */


static dev_t firstDevNbr; // Global variable for the first device number
static struct cdev c_dev; // Global variable for the character device structure
static struct class *cl; // Global variable for the device class

static struct proc_dir_entry *parent;
static struct proc_dir_entry *file;


#define DEFAULT_LED_STRTYPE "WS2812B"
#define DEFAULT_PERIOD_IN_NSEC 50
#define DEFAULT_PERIOD_COUNT 25
#define DEFAULT_T0H_COUNT 8
#define DEFAULT_T1H_COUNT 16
#define DEFAULT_TRESET_COUNT 1000
#define DEFAULT_LOOP_ENABLE 0


static unsigned char ledType[FIFO_MAX_STR_LEN+1] = DEFAULT_LED_STRTYPE; // +1 for zero term.
static int gpioPins[FIFO_MAX_PIN_COUNT];    // max 3 gpio pins can be assigned
static int periodDurationNsec = DEFAULT_PERIOD_IN_NSEC;
static int periodCount = DEFAULT_PERIOD_COUNT;
static int periodT0HCount = DEFAULT_T0H_COUNT;
static int periodT1HCount = DEFAULT_T1H_COUNT;
static int periodTRESETCount = DEFAULT_TRESET_COUNT;
static int loopEnabled = DEFAULT_LOOP_ENABLE;

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


static ssize_t LEDfifo_write(struct file *f, const char __user *buf, size_t len,
    loff_t *off)
{
    printk(KERN_INFO "Driver: write()\n");
    return len;
}

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
            if (copy_from_user(&cfg, (configure_arg_t *)arg,
                sizeof(configure_arg_t)))
            {
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
        case CMD_GET_LOOP_ENABLE:
	    retval = loopEnabled;
	   break;
        case CMD_SET_LOOP_ENABLE:
	    loopEnabled = arg;
	   break;
        default:
            return -EINVAL; // unknown command?  How'd this happen?
    }

    return retval;
}


static struct file_operations LEDfifoLKM_fops =
{
    .owner = THIS_MODULE,
    .open = LEDfifo_open,
    .read = LEDfifo_read,
    .write = LEDfifo_write,
    .release = LEDfifo_close,
    .unlocked_ioctl = LEDfifo_ioctl
};

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



