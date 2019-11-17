/**
 * @file    ioctlSampleApp.c
 * @author  Stephen M Moraco
 * @date    15 November 2019
 * @version 0.1
 * @brief  An introductory "Hello World!" loadable kernel module (LKM) that can display a message
 * in the /var/log/kern.log file when the module is loaded and removed. The module can accept an
 * argument when it is loaded -- the name, which appears in the kernel log files.
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.
*/
 
#include <stdio.h>
#include <sys/ioctl.h>

#include "LEDfifoConfigureIOCtl.h"

void get_vars(int fd)
{
    configure_arg_t q;

    if (ioctl(fd, CMD_GET_VARIABLES, &q) == -1)
    {
        perror("query_app ioctl get");
    }
    else
    {
        printf("LED Type : %d\n", q.ledType);
        //printf("Dignity: %d\n", q.dignity);
        //printf("Ego    : %d\n", q.ego);
    }
}

void clr_vars(int fd)
{
    if (ioctl(fd, CMD_RESET_VARIABLES) == -1)
    {
        perror("query_app ioctl clr");
    }
}
