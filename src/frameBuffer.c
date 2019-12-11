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

#include <stdio.h> 
#include <string.h> 

#include "frameBuffer.h"
#include "xmalloc.h"
#include "debug.h"

#define MAX_BUFFER_POINTERS 50
// setup our master frame buffer
static struct _LedPixel *pFrameBufferAr[MAX_BUFFER_POINTERS + 1]; 
static int nNumberAllocatedBuffers;

static int nLenPanel;
static int nLenFrameBuffer;

void initBuffers(void)
{
	nLenPanel = (sizeof(struct _LedPixel) * LEDS_PER_PANEL);
	nLenFrameBuffer = (nLenPanel * NUMBER_OF_PANELS);
	nNumberAllocatedBuffers = 0;

	// alloc our frame buffers and init to black
	if(pFrameBufferAr[0] == NULL) {
		pFrameBufferAr[0] = xmalloc(nLenFrameBuffer);
		pFrameBufferAr[1] = NULL; // we always have a null pointer at end of list of buffer pointers
		nNumberAllocatedBuffers = 1;
		debugMessage("- Allocated frameBuffer@%p:[%d buffers][%d panels][%d LEDs][%d bytes]\n", pFrameBufferAr[0], 1, NUMBER_OF_PANELS, LEDS_PER_PANEL, sizeof(struct _LedPixel));
	}
}

void clearBuffers(void)
{
    for(int nBffrIdx = 0; nBffrIdx < nNumberAllocatedBuffers; nBffrIdx++) {
    	// write zeros to our entire set of buffers
    	memset(pFrameBufferAr[nBffrIdx], 0, nLenFrameBuffer);
    }
	debugMessage("clearBuffers() - %d Buffers reset to zero", nNumberAllocatedBuffers);
}

int allocBuffers(int nDesiredBuffers)
{
    int allocStatus = 0;    // SUCCESS

    debugMessage("allocBuffers(%d) - ENTRY", nDesiredBuffers);
    if(nDesiredBuffers > MAX_BUFFER_POINTERS) {
	    warningMessage("buffer %d out-of-range: MAX %d supported", nDesiredBuffers, MAX_BUFFER_POINTERS);
    }
    else if(nDesiredBuffers > nNumberAllocatedBuffers) {
	debugMessage("Alloc %d additional buffers", nDesiredBuffers - nNumberAllocatedBuffers);
        for(int nBffrIdx = nNumberAllocatedBuffers; nBffrIdx < nDesiredBuffers; nBffrIdx++) {
		    pFrameBufferAr[nBffrIdx] = xmalloc(nLenFrameBuffer);
		    if(pFrameBufferAr[nBffrIdx] == NULL) {
	                errorMessage("[CODE] failed to allocate buffer %d (of %d), Aborted", nBffrIdx, nDesiredBuffers);
	                allocStatus = -1; // FAILURE
	                break;
		    } 
		    else {
                        debugMessage("allocated buffer[%d] @%p", nBffrIdx,pFrameBufferAr[nBffrIdx]);
		    }
        }
        pFrameBufferAr[nDesiredBuffers] = NULL; // place trailing NULL at end of list
    }
    debugMessage("allocBuffers() - EXIT");
    return allocStatus;
}

uint8_t numberBuffers(void)
{
	return nNumberAllocatedBuffers;
}

uint8_t numberPanels(void)
{
	return NUMBER_OF_PANELS;
}

uint16_t maxLedsInBuffer(void)
{
	return LEDS_PER_PANEL * NUMBER_OF_PANELS;
}

uint16_t maxLedsInPanel(void)
{
	return LEDS_PER_PANEL;
}

struct _LedPixel *ptrBuffer(uint8_t nBuffer)
{
	struct _LedPixel *desiredAddr = NULL;
	
	if(nBuffer <= nNumberAllocatedBuffers) {
		desiredAddr = (struct _LedPixel *)pFrameBufferAr[nBuffer - 1];
	}
	else {
	    if(nBuffer > MAX_BUFFER_POINTERS) {
    	    warningMessage("buffer %d out-of-range: MAX %d supported", nBuffer, MAX_BUFFER_POINTERS);
    	}
    	else {
     	    warningMessage("buffer %d NOT yet Allocated. Use 'buffers %d' to allocate it");
       	}
	}
	return desiredAddr;
}

struct _LedPixel *ptrPanel(struct _LedPixel *pBuffer, uint8_t nPanel)
{
	struct _LedPixel *desiredAddr = NULL;
	
	if(pBuffer != NULL && nPanel < NUMBER_OF_PANELS) {
		desiredAddr = &pBuffer[nPanel * LEDS_PER_PANEL];
	}
	else if(nPanel < NUMBER_OF_PANELS) {
 	    warningMessage("panel %d out-of-range. Max %d panels.", nPanel, NUMBER_OF_PANELS);
	}
	else {
 	    errorMessage("[CODE] ptrPanel(NULL,%d) must specify buffer address.", pBuffer, nPanel);
	}
	return desiredAddr;
}
