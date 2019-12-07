/* 
   matrix - interactive LED Matrix console

   Copyright (C) 2019 Stephen M Moraco

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  

*/

#include <sys/ioctl.h>
#include <fcntl.h>      // for open()
#include <unistd.h>     // for close()
#include <string.h>     // for strxxx()

#include "LEDfifoConfigureIOCtl.h"

#include "matrixDriver.h"
#include "debug.h"

// forward declarations
void show_vars(int fd);
void setPins(int fd, int pinsAr[], int pinCount);
void resetToWS2812bValues(int fd);
void clearToColor(int fd, uint32_t color);

void testSetPins(int fd);


static int s_fdDriver;

static int s_nPinsAr = { 17, 27, 22 };

int openMatrix(void)
{
    int errorValue = 0;
    
    debugMessage("Driver Connect");
    s_fdDriver = open("/dev/ledfifo0", O_RDWR);
    if(s_fdDriver < 0) {
        errorValue = -1;
    }
    else {
        // configure for WS2812B
        resetToWS2812bValues();
        
        // and set our pins
        setPins(s_fdDriver, &s_nPinsAr[0], sizeof(s_nPinsAr));
        
        // reset all pixels to off (black)
        clearToColor(s_fdDriver, 0x000000);
    }
    return errorValue;
}

int closeMatrix(void)
{
    int errorValue = 0;
    
    debugMessage("Driver Disconnect");
    status = close(s_fdDriver);
    if(status != 0) {
        errorValue = -1;
    }
    return errorValue;
}

void showBuffer(uint8_t *buffer, size_t bufferLen)
{
    // write our buffer to the LED matrix for display
    debugMessage("Write buffer %p(%ld) - TODO Implement this!", buffer, bufferLen);
}


// ============================================================================
//  file-static routines (used this file only)
//
void show_vars(int fd)
{
    configure_arg_t deviceValues;
    
    printf("-> get_vars() ENTRY\n");
    
    if (ioctl(fd, CMD_GET_VARIABLES, &deviceValues) == -1)
    {
        perror("testApp ioctl get");
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

void setPins(int fd, int pinsAr[], int pinCount)
{
    configure_arg_t deviceValues;
    
    debugMessage("-> setPins() ENTRY");
    
    if(pinCount != 3) {
        errorMessage("setPins() This device expects to have 3 pins!");
    }
    else {
        if (ioctl(fd, CMD_GET_VARIABLES, &deviceValues) == -1)
        {
            perror("setPins()  ioctl get");
        }
        else
        { 
            deviceValues.gpioPins[0] = pinsAr[0];
            deviceValues.gpioPins[1] = pinsAr[1];
            deviceValues.gpioPins[2] = pinsAr[2];
            if (ioctl(fd, CMD_SET_VARIABLES, &deviceValues) == -1)
            {
                perror("setPins()  ioctl set");
            }
        }
    }
    debugMessage("-> setPins() EXIT");
}

void resetToWS2812bValues(int fd)
{
    debugMessage("-> resetToWS2812B() ENTRY");

    if (ioctl(fd, CMD_RESET_VARIABLES) == -1)
    {
        perror("resetToWS2812B() ioctl clr");
    }
    debugMessage("-- resetToWS2812B() EXIT");
}

void clearToColor(int fd, uint32_t color)
{
    debugMessage("-> clearToColor(0x%.06X) ENTRY", color);

    if (ioctl(fd, CMD_SET_SCREEN_COLOR, color) == -1)
    {
        perror("clearToColor() ioctl fill w/color");
    }
    debugMessage("-- clearToColor() EXIT");
}


// ============================================================================
// Example TEST code
//
void testSetPins(int fd)
{
    configure_arg_t deviceValues;
    
    printf("-> testSetPins() ENTRY\n");
    if (ioctl(fd, CMD_GET_VARIABLES, &deviceValues) == -1)
    {
        perror("testApp ioctl get");
    }
    else
    { 
        if(deviceValues.gpioPins[0] == 0) {

            deviceValues.gpioPins[0] = 17;
            deviceValues.gpioPins[1] = 27;
            deviceValues.gpioPins[2] = 22;
            if (ioctl(fd, CMD_SET_VARIABLES, &deviceValues) == -1)
            {
                perror("testApp ioctl set");
            }
            else {
                if (ioctl(fd, CMD_GET_VARIABLES, &deviceValues) == -1)
                {
                    perror("testApp ioctl get");
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
        else {
          printf(" - pin set SKIPPED, already set\n");
	}
    }

    printf("-- testSetPins() EXIT\n\n");

}

void testBySendingBits(int fd, int value)
{
    printf("-> testBySendingBits(%d) ENTRY\n", value);

    if (ioctl(fd, CMD_TEST_BIT_WRITES, value) == -1)
    {
        perror("testApp ioctl set bit to 0/1");
    }
    printf("-- testBySendingBits() EXIT\n\n");
}

void testBySendingColor(int fd, int value)
{
    printf("-> testBySendingColor(0x%.6X) ENTRY\n", value);

    if (ioctl(fd, CMD_SET_SCREEN_COLOR, value) == -1)
    {
        perror("testApp ioctl fill w/color");
    }
    printf("-- testBySendingColor() EXIT\n\n");
}

void testSet2815(int fd)
{
    configure_arg_t deviceValues;
    
    printf("-> testSetPins() ENTRY\n");

    strcpy(deviceValues.ledType, "WS2815\0");
    deviceValues.gpioPins[0] = 17;
    deviceValues.gpioPins[1] = 27;
    deviceValues.gpioPins[2] = 22;
    deviceValues.periodDurationNsec = 50;
    deviceValues.periodCount = 27;
    deviceValues.periodT0HCount = 6;
    deviceValues.periodT1HCount = 21;
    deviceValues.periodTRESETCount = 5600;
    if (ioctl(fd, CMD_SET_VARIABLES, &deviceValues) == -1)
    {
        perror("testApp ioctl set");
    }
    else {
        printf("- TEST PASS\n");
    }
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
        perror("testApp ioctl SET LOOP");
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
