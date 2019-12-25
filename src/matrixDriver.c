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

#include <LEDfifoLKM/LEDfifoConfigureIOCtl.h>

#include "matrixDriver.h"
#include "debug.h"

// forward declarations
void show_vars(int fd);
void setPins(int fd, int pinsAr[], int pinCount);
int identifyPiModel(int fd);
void resetToWS2812bValues(int fd);
void clearToColor(int fd, uint32_t color);
int setIOBaseAddress(int fd, uint32_t baeeAddress);

void testSetPins(int fd);

enum pitypes {NOTSET,ARM6,ARM7,PI4};
static enum pitypes s_ePiType = NOTSET;  // set by identifyPiModel()  0=not set 1=ARMv6  2=ARMv7, etc.

static int s_fdDriver;

static int s_nPinsAr[3] = { 17, 27, 22 };

int openMatrix(void)
{
    int errorValue = -1; // success

    debugMessage("Driver Connect");
    s_fdDriver = open("/dev/ledfifo0", O_RDWR);
    if(s_fdDriver < 0) {
        errorValue = 0;	// error
    }
    else {
        // configure for RPi Model
        int status = identifyPiModel(s_fdDriver);
        if(status) {
            // configure for WS2812B
            resetToWS2812bValues(s_fdDriver);

            // and set our pins
            setPins(s_fdDriver, &s_nPinsAr[0], sizeof(s_nPinsAr)/sizeof(int));

            // reset all pixels to off (black)
            clearToColor(s_fdDriver, 0x000000);
        }
        else {
            errorMessage("openMatrix() failed to ID RPi Model");
            errorValue = 0;	// error
        }

    }
    return errorValue;
}

int identifyPiModel(int fd)
{
    s_ePiType = NOTSET;  // in case fails

    uint32_t baseadd = 0;

    FILE *stream = fopen("/proc/device-tree/soc/ranges","rb");

    if(stream == NULL) {
        errorMessage("Failed to open /proc/device-tree/soc/ranges");
        return(0); // failed identify
    }

    int getout = 0;
    int n = 0;
    do {
        int c = getc(stream);
        if(c == EOF) {
            baseadd = 0;
            getout = 1;
        }
        else if(n > 3) {
            baseadd = (baseadd << 8) + c;
        }
        ++n;
        if(n == 8 && baseadd == 0) {
            n = 4;  // read third 4 bytes
        }
    } while(getout == 0 && n < 8);
    fclose(stream);

    // now determine this model's IO base address
    if(baseadd == 0x20000000) {
        s_ePiType = ARM6;   // Pi2
        debugMessage("RPi type = ARMv6");
    }
    else if(baseadd == 0x3F000000) {
        s_ePiType = ARM7;  // Pi3B+
        debugMessage("RPi type = ARMv7");
    }
    else if(baseadd == 0xFE000000) {
        s_ePiType = PI4;   // Pi4
        debugMessage("RPi type = Pi4");
    }
    else {
        errorMessage("Failed to determine RPi type");
        return(0); // failed identify
    }

    // now inform our driver!
    if(!setIOBaseAddress(s_fdDriver, baseadd)) {
        return(0); // failed identify
    }

    return(1);
}

int closeMatrix(void)
{
    int status;
    int errorValue = -1; // success

    debugMessage("Driver Disconnect");
    status = close(s_fdDriver);
    if(status != 0) {
        errorValue = 0;	// error
    }
    return errorValue;
}

void showBuffer(uint8_t *buffer, size_t bufferLen)
{
    // write our buffer to the LED matrix for display
    //debugMessage("showBuffer() %p(%ld) - ENTRY", buffer, bufferLen);
    ssize_t numberBytesWritten = write(s_fdDriver, buffer, bufferLen);
    if(numberBytesWritten == -1) {
        perrorMessage("write() failed");
    }
    else if(numberBytesWritten != bufferLen) {
        warningMessage("showBuffer() ONLY write %d of %d bytes!", numberBytesWritten, bufferLen);
    }
    //debugMessage("showBuffer() - EXIT");
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
        errorMessage("setPins() This device expects to have 3 pins (got %d)!", pinCount);
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

int setIOBaseAddress(int fd, uint32_t baseAddress)
{
    int returnStatus = -1; // SUCCESS
    debugMessage("-> setIOBaseAddress(0x%.08X) ENTRY", baseAddress);

    if (ioctl(fd, CMD_SET_IO_BASE_ADDRESS, baseAddress) == -1)
    {
        perror("setIOBaseAddress() ioctl set baseAddr");
        returnStatus = 0; // FAILURE
    }
    debugMessage("-- setIOBaseAddress() EXIT");
    return returnStatus;
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
