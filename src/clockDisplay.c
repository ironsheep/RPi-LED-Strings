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

#include "clockDisplay.h"
#include "debug.h"

// forward declarations
static void *threadDigitalClock(void *argp);
static void *threadBinaryClock(void *argp);


static pthread_t clockThread;
static int bClockRunning;

void runClock(eClockFaceTypes clockType, utint32_t nFaceColor)
{
    int status;

    verboseMessage("runClock() Start Clock Thread");
    switch(clockType) {
        case CFT_DIGITAL:
            status = pthread_create(&clockThread, NULL, &threadDigitalClock, nFaceColor);
            if (status != 0) {
                perrorMessage("pthread_create(Digital) failure");
            }
            break;
        case CFT_BINARY:
            status = pthread_create(&clockThread, NULL, &threadBinaryClock, nFaceColor);
            if (status != 0) {
                perrorMessage("pthread_create(Binary) failure");
            }
            break;
        default:
            errorMessage("runClock() Unknown clock type (%d)", clockType);
            break
    }
}

void stopClock(void)
{
    verboseMessage("stopClock() Stop Clock Thread");
    if(bClockRunning) {
        int status = pthread_cancel(clockThread);
        if (status != 0) {
            perrorMessage("stopClock() pthread_cancel() failure");
        }
    }
    bClockRunning = 0;  // show NOT running
}

int isClockRunning(void)
{
    return bClockRunning;
}

// ---------------------------------------------------------------------------
// Clock Thread Functions
//
static void *threadDigitalClock(void *argp)
{
    bClockRunning = 1;  // show IS running

    int status = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if(status != 0) {
        perrorMessage("threadDigitalClock() pthread_setcancelstate() failure");
    }

    do {
        showCurrDigitalFace();
        //sleep 1 second
    }
}

static void *threadBinaryClock(void *argp)
{
    bClockRunning = 1;  // show IS running

    int status = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if(status != 0) {
        perrorMessage("threadDigitalClock() pthread_setcancelstate() failure");
    }

    do {
        showCurrBinaryFace();
        //sleep 1 second
    }
}

