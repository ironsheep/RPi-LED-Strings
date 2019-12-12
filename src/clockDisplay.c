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

#include <pthread.h>
#include <time.h>

#include "clockDisplay.h"
#include "frameBuffer.h"
#include "matrixDriver.h"
#include "debug.h"


typedef enum _eTimeUnits {
    TU_Unknown = 0,
    TU_SECONDS,
    TU_MINUTES,
    TU_HOURS,
} eTimeUnits;


// forward declarations
void delayMilliSec(int nMilliSec);
void *threadDigitalClock(void *argp);
void *threadBinaryClock(void *argp);
void showCurrDigitalFace(uint32_t nFaceColor);
void showCurrBinaryFace(uint32_t nFaceColor);
void updateBinaryFace(eTimeUnits tmUnits, int tmValue, uint32_t nFaceColor);
void placeTensUnits(int nValue, uint8_t locX, uint8_t locY, uint32_t nFaceColor);
void placeBit(uint8_t bValue, uint8_t locX, uint8_t locY, uint32_t nFaceColor);
void placeVertBar(uint8_t locX, uint8_t locY);


static pthread_t s_clockThread;
static int s_bClockRunning;
static uint32_t s_nFaceColor;
static int s_nClockBufferNumber;

void runClock(eClockFaceTypes clockType, uint32_t nFaceColor, int nBufferNumber)
{
    int status;

    s_nFaceColor = nFaceColor;
    s_nClockBufferNumber = nBufferNumber;

    verboseMessage("runClock() Start Clock Thread");
    switch(clockType) {
        case CFT_DIGITAL:
            status = pthread_create(&s_clockThread, NULL, &threadDigitalClock, (void *)nFaceColor);
            if (status != 0) {
                perrorMessage("pthread_create(Digital) failure");
            }
            break;
        case CFT_BINARY:
            status = pthread_create(&s_clockThread, NULL, &threadBinaryClock, (void *)nFaceColor);
            if (status != 0) {
                perrorMessage("pthread_create(Binary) failure");
            }
            break;
        default:
            errorMessage("runClock() Unknown clock type (%d)", clockType);
            break;
    }
}

void stopClock(void)
{
    verboseMessage("stopClock() Stop Clock Thread");
    if(s_bClockRunning) {
        int status = pthread_cancel(s_clockThread);
        if (status != 0) {
            perrorMessage("stopClock() pthread_cancel() failure");
        }
    }
    s_bClockRunning = 0;  // show NOT running
}

int isClockRunning(void)
{
    return s_bClockRunning;
}

// ---------------------------------------------------------------------------
// Clock Thread Functions
//
void *threadDigitalClock(void *argp)
{
    s_bClockRunning = 1;  // show IS running

    // ensure clock face is blanked, once
    fillBufferWithColorRGB(s_nClockBufferNumber ,0x000000);

    // allow this thread to be cancelled
    int status = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if(status != 0) {
        perrorMessage("threadDigitalClock() pthread_setcancelstate() failure");
    }

    do {
        showCurrDigitalFace((uint32_t)argp);
        //sleep 1 second
        delayMilliSec(950);

    } while(1);
}

void *threadBinaryClock(void *argp)
{
    s_bClockRunning = 1;  // show IS running

    // ensure clock face is blanked, once
    fillBufferWithColorRGB(s_nClockBufferNumber ,0x000000);

    // allow this thread to be cancelled
    int status = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if(status != 0) {
        perrorMessage("threadDigitalClock() pthread_setcancelstate() failure");
    }

    do {

        showCurrBinaryFace((uint32_t)argp);
        //sleep 1 second
        delayMilliSec(950);

    } while(1);
}

void showCurrDigitalFace(uint32_t nFaceColor)
{
    //
    // get current time to the second
    //
    // get seconds since the Epoch
    time_t secs = time(0);

    // convert to localtime
    struct tm *local = localtime(&secs);

    // FIXME: UNDONE need logic here for Digital Clock
}

void delayMilliSec(int nMilliSec)
{
    // get start time
    clock_t start_time = clock();
    // add offset
    clock_t end_time = start_time + nMilliSec;
    // looping till required time is not acheived
    while (clock() < end_time);
}

typedef enum _eObjIndex {
    OI_HrTens = 0,
    OI_HrUnits,
    OI_Bar_Left,
    OI_MinTens,
    OI_MinUnits,
    OI_Bar_Right,
    OI_SecTens,
    OI_SecUnits,
} eObjIndex;

struct _objLocnXY {
    uint8_t objId;
    uint8_t X;
    uint8_t Y;
} locTable[] = {
     { OI_HrTens, 4, 5 },
     { OI_HrUnits, 8, 5 },
     { OI_Bar_Left, 11, 5 },
     { OI_MinTens, 13, 5 },
     { OI_MinUnits, 17, 5 },
     { OI_Bar_Right, 19, 5 },
     { OI_SecTens, 21, 5 },
     { OI_SecUnits, 25, 5 },
};

void showCurrBinaryFace(uint32_t nFaceColor)
{
    //
    // get current time to the second
    //
    // get seconds since the Epoch
    time_t secs = time(0);

    // convert to localtime
    struct tm *local = localtime(&secs);

    updateBinaryFace(TU_SECONDS, local->tm_sec, nFaceColor);
    updateBinaryFace(TU_MINUTES, local->tm_min, nFaceColor);
    updateBinaryFace(TU_HOURS, local->tm_hour, nFaceColor);

    // update blinking bars
    placeVertBar(locTable[OI_Bar_Left].X, locTable[OI_Bar_Left].Y);
    placeVertBar(locTable[OI_Bar_Right].X, locTable[OI_Bar_Right].Y);

    // now write buffer N contents to matrix itself
    int nBufferSize = frameBufferSizeInBytes();
    uint8_t *pCurrBuffer = (uint8_t *)ptrBuffer(s_nClockBufferNumber);
    showBuffer(pCurrBuffer, nBufferSize);
}


void updateBinaryFace(eTimeUnits tmUnits, int tmValue, uint32_t nFaceColor)
{
    switch(tmUnits) {
        case TU_SECONDS:
            placeTensUnits(tmValue, locTable[OI_SecTens].X, locTable[OI_SecTens].Y, nFaceColor);
            break;
        case TU_MINUTES:
            placeTensUnits(tmValue, locTable[OI_MinTens].X, locTable[OI_MinTens].Y, nFaceColor);
            break;
        case TU_HOURS:
            placeTensUnits(tmValue, locTable[OI_HrTens].X, locTable[OI_HrTens].Y, nFaceColor);
            break;
        default:
            break;
    }
}

void placeTensUnits(int nValue, uint8_t locX, uint8_t locY, uint32_t nFaceColor)
{
    uint8_t bit7 = (nValue & 0x0080) > 0;
    placeBit(bit7, locX, locY, nFaceColor);
    uint8_t bit6 = (nValue & 0x0040) > 0;
    placeBit(bit6, locX, locY+4, nFaceColor);
    uint8_t bit5 = (nValue & 0x0020) > 0;
    placeBit(bit5, locX, locY+8, nFaceColor);
    uint8_t bit4 = (nValue & 0x0010) > 0;
    placeBit(bit4, locX, locY+12, nFaceColor);
    uint8_t bit3 = (nValue & 0x0008) > 0;
    placeBit(bit3, locX+4, locY, nFaceColor);
    uint8_t bit2 = (nValue & 0x0004) > 0;
    placeBit(bit2, locX+4, locY+4, nFaceColor);
    uint8_t bit1 = (nValue & 0x0002) > 0;
    placeBit(bit1, locX+4, locY+8, nFaceColor);
    uint8_t bit0 = (nValue & 0x0001) > 0;
    placeBit(bit0, locX+4, locY+12, nFaceColor);
}

void placeBit(uint8_t bValue, uint8_t locX, uint8_t locY, uint32_t nFaceColor)
{
    // set 4 LEDs 2x2 to same color (faceColor -OR- off)
    uint32_t nColor = (bValue == 0) ? 0x000000 : nFaceColor;
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY);
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX+1, locY);
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY+1);
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX+1, locY+1);
}

static int bBarLight = 0;

void placeVertBar(uint8_t locX, uint8_t locY)
{
    // set two vert pix to lt or dk gray
    bBarLight = (bBarLight == 0) ? 1 : 0;
    uint32_t nColor = (bBarLight == 0) ? 0x404040 : 0x808080;
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY);
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY+1);
}
