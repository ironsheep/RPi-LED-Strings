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
#include <fcntl.h>  // for open()
#include <unistd.h>     // for close
#include <string.h>     // for strxxx()

#include "LEDfifoConfigureIOCtl.h"

// forward declarations
void get_vars(int fd);
void clr_vars(int fd);
void set_vars(int fd);
void testLOOPingControl(int fd);
void testSetPins(int fd);
void testSet2815(int fd);

// test app
int main()
{
    int fd;

    printf("\nOpening Driver\n");
    fd = open("/dev/ledfifo0", O_RDWR);
    if(fd < 0) {
        printf("ERROR: Failed to open device file...\n");
        return -1;
    }

    get_vars(fd);

    testLOOPingControl(fd);

    testSetPins(fd);

    testSet2815(fd);
    get_vars(fd);

    clr_vars(fd);
    get_vars(fd);

    printf("Closing Driver\n");
    close(fd);
}


void get_vars(int fd)
{
    configure_arg_t deviceValues;

    printf("-> get_vars() ENTRY\n");

    if (ioctl(fd, CMD_GET_VARIABLES, &deviceValues) == -1)
    {
        perror("query_app ioctl get");
    }
    else
    {
        printf(" - LED Type: [%s]\n", deviceValues.ledType);
        for(int pinIndex=0; pinIndex<FIFO_MAX_PIN_COUNT; pinIndex++) {
            if(deviceValues.gpioPins[pinIndex] != 0) {
                printf(" - Pin #%d: GPIO %d\n", pinIndex+1, deviceValues.gpioPins[pinIndex]);
             }
             else {
                printf(" - Pin #%d: {notSet}\n", pinIndex+1);
            }
        }
        float freqInKHz = 1.0 / (deviceValues.periodCount * deviceValues.periodDurationNsec * 0.000000001) / 1000.0;
        printf(" - LED String: %.3f KHz: %d nSec period (%dx %d nSec sub-periods)\n", freqInKHz, (deviceValues.periodCount * deviceValues.periodDurationNsec), deviceValues.periodCount, deviceValues.periodDurationNsec);
        printf("      - Bit 0: T0H %d nSec, T0L %d nSec\n", deviceValues.periodT0HCount, deviceValues.periodCount - deviceValues.periodT0HCount);
        printf("      - Bit 1: T0H %d nSec, T0L %d nSec\n", deviceValues.periodT1HCount, deviceValues.periodCount - deviceValues.periodT1HCount);
        printf("      - RESET: %.1f uSec\n", (deviceValues.periodTRESETCount * deviceValues.periodDurationNsec) / 1000.0);
    }
    printf("-- get_vars() EXIT\n\n");
}

void testSetPins(int fd)
{
    configure_arg_t deviceValues;

    printf("-> testSetPins() ENTRY\n");
    if (ioctl(fd, CMD_GET_VARIABLES, &deviceValues) == -1)
    {
        perror("query_app ioctl get");
    }
    else
    {
        deviceValues.gpioPins[0] = 17;
        deviceValues.gpioPins[1] = 27;
        deviceValues.gpioPins[2] = 22;
        if (ioctl(fd, CMD_SET_VARIABLES, &deviceValues) == -1)
        {
            perror("query_app ioctl set");
        }
        else {
            if (ioctl(fd, CMD_GET_VARIABLES, &deviceValues) == -1)
            {
                perror("query_app ioctl get");
            }
            else {
                if(deviceValues.gpioPins[0] != 0 && deviceValues.gpioPins[1] != 0 && deviceValues.gpioPins[2] != 0) {
                    printf("- TEST PASS\n");
                }
                else {
                    printf("- TEST FAILURE!!\n");
                }
            }
        }
    }

    printf("-- testSetPins() EXIT\n\n");

}

void testSet2815(int fd)
{
    configure_arg_t deviceValues;

    printf("-> testSetPins() ENTRY\n");

    strcpy(deviceValues.ledType, "WS2815\0");
    deviceValues.gpioPins[0] = 17;
    deviceValues.gpioPins[1] = 27;
    deviceValues.gpioPins[2] = 22;
    deviceValues.periodDurationNsec = 51;
    deviceValues.periodCount = 27;
    deviceValues.periodT0HCount = 6;
    deviceValues.periodT1HCount = 21;
    deviceValues.periodTRESETCount = 5545;
    if (ioctl(fd, CMD_SET_VARIABLES, &deviceValues) == -1)
    {
        perror("query_app ioctl set");
    }
    else {
        printf("- TEST PASS\n");
    }
}

void clr_vars(int fd)
{
    printf("-> clr_vars() ENTRY\n");

    if (ioctl(fd, CMD_RESET_VARIABLES) == -1)
    {
        perror("query_app ioctl clr");
    }
    printf("-- clr_vars() EXIT\n\n");
}

void testLOOPingControl(int fd)
{
    printf("-> testLOOPingControl() ENTRY\n");
    int loopStatusBefore = ioctl(fd, CMD_GET_LOOP_ENABLE);
    printf(" - loop Enable (before): %d\n", loopStatusBefore);

    int testValue = (loopStatusBefore == 0) ? -1 : 0;

    // write alternate value
    if(ioctl(fd, CMD_SET_LOOP_ENABLE, testValue) == -1)
    {
        perror("query_app ioctl SET LOOP");
    }

    // check value on readback
    int loopStatusAfter = ioctl(fd, CMD_GET_LOOP_ENABLE);
    printf(" - loop Enable (after): %d\n", loopStatusAfter);

    if(loopStatusAfter == testValue) {
        printf("- TEST PASS\n");
    }
    else {
        printf("- TEST FAILURE!!\n");
    }

    printf("-- testLOOPingControl() EXIT\n\n");
}
