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

#include "xmalloc.h"
#include "frameBuffer.h"


// setup our master frame buffer
static struct _LedPixel *pFrameBuffers; // [NUMBER_OF_BUFFERS][NUMBER_OF_PANELS][LEDS_PER_PANEL];
static int nLenPanel;
static int nLenBuffer;
static int nLenFrameBuffers;

void initBuffers(void)
{
	nLenPanel = (sizeof(struct _LedPixel) * LEDS_PER_PANEL);
	nLenBuffer = (nLenPanel * NUMBER_OF_PANELS);
	nLenFrameBuffers = (nLenBuffer * NUMBER_OF_BUFFERS);

	// alloc our frame buffers and init to black
	if(pFrameBuffers == NULL) {
		pFrameBuffers = xmalloc(nLenFrameBuffers);
		printf("- Allocated frameBuffer@%p:[%d buffers][%d panels][%d LEDs][%d bytes]\n", pFrameBuffers, NUMBER_OF_BUFFERS, NUMBER_OF_PANELS, LEDS_PER_PANEL, sizeof(struct _LedPixel));
	}
}

void clearBuffers(void)
{
	// write zeros to our entire set of buffers
	memset(pFrameBuffers, 0, nLenFrameBuffers);
	printf("- Buffers reset to zero\n");
}

uint8_t numberBuffers(void)
{
		return NUMBER_OF_BUFFERS;
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
	
	if(nBuffer < NUMBER_OF_BUFFERS) {
		struct _LedPixel *allBuffers = (struct _LedPixel *)pFrameBuffers;
		desiredAddr = (struct _LedPixel *)&allBuffers[nBuffer * (LEDS_PER_PANEL * NUMBER_OF_PANELS)];
	}
	return desiredAddr;
}

struct _LedPixel *ptrPanel(struct _LedPixel *pBuffer, uint8_t nPanel)
{
	struct _LedPixel *desiredAddr = NULL;
	
	if(pBuffer != NULL && nPanel < NUMBER_OF_PANELS) {
		struct _LedPixel *currBuffer = pBuffer;
		desiredAddr = &currBuffer[nPanel * LEDS_PER_PANEL];
	}
	return desiredAddr;
}
