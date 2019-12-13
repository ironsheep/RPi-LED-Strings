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
static eClockFaceTypes s_nClockType;

#define USE_TIMER

#ifdef USE_PTHREADS
void runClock(eClockFaceTypes clockType, uint32_t nFaceColor, int nBufferNumber)
{
    int status;

    s_nFaceColor = nFaceColor;
    s_nClockBufferNumber = nBufferNumber;
    s_nClockType = clockType;

    verboseMessage("runClock() Start Clock THread");
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
#endif

#ifdef USE_TIMER
void runClock(eClockFaceTypes clockType, uint32_t nFaceColor, int nBufferNumber)
{
    const long intervalInMilliSec = 1000;

    s_nFaceColor = nFaceColor;
    s_nClockBufferNumber = nBufferNumber;
    s_nClockType = clockType;

    verboseMessage("runClock() Start Clock Timer");
    if(clockType == CFT_DIGITAL) {
        warningMessage("runClock() clock type (%d) NOT YET SUPPORTED", clockType);
    }
    else if(clockType == CFT_BINARY) {
        if(!s_bClockRunning) {
            s_bClockRunning = 1;  // show IS running
            create_timer(periodInMilliSec);
        }
        else {
            warningMessage("runClock() Skipped, already running (use'clock stop' before next start)");
        }
    }
    else {
        errorMessage("runClock() Unknown clock type (%d)", clockType);
    }
}

void stopClock(void)
{
    verboseMessage("stopClock() Stop Clock Thread");

    if(s_bClockRunning) {
        destroy_timer();
        s_bClockRunning = 0;  // show NOT running
    }
    else {
        warningMessage("stopClock() no clock running!");
    }
}

#endif

int isClockRunning(void)
{
    return s_bClockRunning;
}

// ============================================================================
//

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void handleTimerExpiration(union sigval arg)
{
    int status;

    status = pthread_mutex_lock(&mutex);
    if (status != 0) {
        errorMessage("handleTimerExpiration(): failed to Lock mutex error(%d)", status);
    }

    // task code run on timer expire...
    if(s_nClockType == CFT_BINARY) {
        showCurrBinaryFace(s_nFaceColor);
    }

    status = pthread_mutex_unlock(&mutex);
    if (status != 0) {
        errorMessage("handleTimerExpiration(): failed to Unlock mutex error(%d)", status);
    }
}

static timer_t timer_id;

void create_timer(long mSeconds)
{
    int status;
    struct itimerspec ts;
    struct sigevent se;
    long long nanosecs = mSeconds * 1000 * 1000;

    /*
    * Set the sigevent structure to cause the signal to be
    * delivered by creating a new thread.
    */
    se.sigev_notify = SIGEV_THREAD;
    se.sigev_value.sival_ptr = &timer_id;
    se.sigev_notify_function = handleTimerExpiration;
    se.sigev_notify_attributes = NULL;

    ts.it_value.tv_sec = nanosecs / 1000000000;
    ts.it_value.tv_nsec = nanosecs % 1000000000;
    ts.it_interval.tv_sec = ts.it_value.tv_sec;
    ts.it_interval.tv_nsec = ts.it_value.tv_nsec;

    status = timer_create(CLOCK_REALTIME, &se, &timer_id);
    if (status == -1) {
        perrorMessage("create_timer(): timer_create() failed");
    }

    status = timer_settime(timer_id, 0, &ts, 0);
    if (status == -1) {
        perrorMessage("create_timer(): timer_settime() failed");
    }
}

void destroy_timer(void)
{
    timer_delete(timer_id);
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
        //delayMilliSec(950);

    } while(0);
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
        // sleep 1 second
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

#define TWObyTHREE

struct _objLocnXY {
    uint8_t objId;
    uint8_t X;
    uint8_t Y;
} locTable[] = {
#ifndef TWObyTHREE
     { OI_HrTens, 4, 5 },
     { OI_HrUnits, 8, 5 },
     { OI_Bar_Left, 11, 7 },
     { OI_MinTens, 13, 5 },
     { OI_MinUnits, 17, 5 },
     { OI_Bar_Right, 20, 7 },
     { OI_SecTens, 22, 5 },
     { OI_SecUnits, 26, 5 },
#else
     { OI_HrTens, 4, 3 },
     { OI_HrUnits, 8, 3 },
     { OI_Bar_Left, 11, 6 },
     { OI_MinTens, 13, 3 },
     { OI_MinUnits, 17, 3 },
     { OI_Bar_Right, 20, 6 },
     { OI_SecTens, 22, 3 },
     { OI_SecUnits, 26, 3 },
#endif
};

#define INT_TO_BCD(decimalValue) ((((decimalValue / 10) << 4) & 0xf0) + ((decimalValue % 10) & 0x0f))

void showCurrBinaryFace(uint32_t nFaceColor)
{
    //
    // get current time to the second
    //
    // get seconds since the Epoch
    time_t secs = time(0);

    // convert to localtime
    struct tm *local = localtime(&secs);

    // and convert h:n:s to bcd
    int hourBCD = INT_TO_BCD(local->tm_hour);
    int minBCD = INT_TO_BCD(local->tm_min);
    int secBCD = INT_TO_BCD(local->tm_sec);

    //debugMessage("tm_hour=%d, hourBCD=0x%.2X", local->tm_hour, hourBCD);
    //debugMessage("tm_min=%d, minBCD=0x%.2X", local->tm_min, minBCD);
    //debugMessage("tm_sec=%d, secBCD=0x%.2X", local->tm_sec, secBCD);

#ifdef TEST_PLACEMENT
    updateBinaryFace(TU_SECONDS, 0x0ff, 0xff0000);
    updateBinaryFace(TU_MINUTES, 0x0ff, 0x00ff00);
    updateBinaryFace(TU_HOURS, 0x0ff, 0x0000ff);
#else
    updateBinaryFace(TU_SECONDS, secBCD, nFaceColor);
    updateBinaryFace(TU_MINUTES, minBCD, nFaceColor);
    updateBinaryFace(TU_HOURS, hourBCD, nFaceColor);
#endif

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
#ifndef TWObyTHREE
    const uint8_t rowOffset = 4;
#else
    const uint8_t rowOffset = 5;
#endif
    uint8_t bit7 = (nValue & 0x0080) > 0;
    placeBit(bit7, locX, locY, nFaceColor);
    uint8_t bit6 = (nValue & 0x0040) > 0;
    placeBit(bit6, locX, locY+(1*rowOffset), nFaceColor);
    uint8_t bit5 = (nValue & 0x0020) > 0;
    placeBit(bit5, locX, locY+(2*rowOffset), nFaceColor);
    uint8_t bit4 = (nValue & 0x0010) > 0;
    placeBit(bit4, locX, locY+(3*rowOffset), nFaceColor);
    uint8_t bit3 = (nValue & 0x0008) > 0;
    placeBit(bit3, locX+4, locY, nFaceColor);
    uint8_t bit2 = (nValue & 0x0004) > 0;
    placeBit(bit2, locX+4, locY+(1*rowOffset), nFaceColor);
    uint8_t bit1 = (nValue & 0x0002) > 0;
    placeBit(bit1, locX+4, locY+(2*rowOffset), nFaceColor);
    uint8_t bit0 = (nValue & 0x0001) > 0;
    placeBit(bit0, locX+4, locY+(3*rowOffset), nFaceColor);
}


void placeBit(uint8_t bValue, uint8_t locX, uint8_t locY, uint32_t nFaceColor)
{
    uint32_t nColor = (bValue == 0) ? 0x010101 : nFaceColor;
    // set 4 LEDs 2x2 to same color (faceColor -OR- off)
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY);
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX+1, locY);
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY+1);
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX+1, locY+1);
#ifdef TWObyTHREE
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY+2);
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX+1, locY+2);
#endif
}

static int bBarLight = 0;

#define BAR_COLOR(bit) ((bit == 0) ? 0x040404 : 0x0A0A0A)

void placeVertBar(uint8_t locX, uint8_t locY)
{
    // set two vert pix to lt or dk gray
    bBarLight = (bBarLight == 0) ? 1 : 0;
    uint32_t nColor = BAR_COLOR(bBarLight);
    // upper  bar
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY);
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY+1);
    // lower bar
    bBarLight = (bBarLight == 0) ? 1 : 0;
    nColor = BAR_COLOR(bBarLight);
#ifndef TWObyTHREE
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY+8);
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY+9);
#else
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY+10);
    setBufferLEDColor(s_nClockBufferNumber, nColor, locX, locY+11);
#endif
}
