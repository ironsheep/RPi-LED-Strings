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

// forward declarations
void get_vars(int fd);
void clr_vars(int fd);
void set_vars(int fd);
void testLOOPingControl(int fd);

// test app
int main()
{
        int fd;
        int32_t value;
        int32_t number;
        
        printf("\nOpening Driver\n");
        fd = open("/dev/ledfifo0", O_RDWR);
        if(fd < 0) {
                printf("Cannot open device file...\n");
                return -1;
        }
 
        testLOOPingControl(fd);
  
        printf("Closing Driver\n");
        close(fd);
}


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

void testLOOPingControl(int fd)
{
    printf("testLOOPingControl() ENTRY\n");
    int loopStatusBefore = ioctl(fd, CMD_GET_LOOP_ENABLE);
    printf("- loop Enable (before): %d\n", loopStatusBefore);
    
    int testValue = (loopStatusBefore == 0) ? -1 : 0;
    
    // write alternate value
    if((ioctl(fd, CMD_SET_LOOP_ENABLE, testValue) == -1)
    {
        perror("query_app ioctl SET LOOP");
    }
    
    // check value on readback
    int loopStatusAfter = ioctl(fd, CMD_GET_LOOP_ENABLE);
    printf("- loop Enable (after): %d\n", loopStatusAfter);
    
    if(loopStatusAfter == testValue) {
        printf("- TEST PASS\n");
    }
    else {
        printf("- TEST FAILURE!!\n");
    }
    
    printf("testLOOPingControl() EXIT\n");
}
