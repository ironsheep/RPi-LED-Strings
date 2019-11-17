#ifndef LED_FIFO_CONFIGURE_IOCTL_H
#define LED_FIFO_CONFIGURE_IOCTL_H

#include <linux/ioctl.h>

#define FIFO_MAX_STR_LEN 15
#define FIFO_MAX_PIN_COUNT 3

typedef struct _configure
{
    unsigned char ledType[FIFO_MAX_STR_LEN+1]; // +1 for zero term.
    int gpioPins[FIFO_MAX_PIN_COUNT];   // max 3 gpio pins can be assigned
    int periodDurationNsec;
    int periodCount;
    int periodT0HCount;
    int periodT1HCount;
    int periodTRESETCount;
} configure_arg_t;

#define LED_FIFO_IOC_MAGIC 'e'

#define CMD_GET_VARIABLES _IOR(LED_FIFO_IOC_MAGIC, 1, configure_arg_t *)
#define CMD_SET_VARIABLES _IOW(LED_FIFO_IOC_MAGIC, 2, configure_arg_t *) 
#define CMD_RESET_VARIABLES _IO(LED_FIFO_IOC_MAGIC, 3)

#define LED_FIFO_IOC_MAXNR 3

#endif  // LED_FIFO_CONFIGURE_IOCTL_H
