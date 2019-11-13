/* 
   led_test_001 - testbed for GPIO and WS2812B String

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
#include <stdbool.h>		// we use the bool type!!!
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>  //Header file for sleep(). man 3 sleep for details.

#include "ledScreen.h"
#include "frameBuffer.h"	// screen sizes too
#include "imageLoader.h"
#include "ledGPIO.h"
#include "xmalloc.h"

static int fileXlateMatrix[NUMBER_OF_PANELS * LEDS_PER_PANEL * BYTES_PER_LED];

static bool bThreadRun = false;

sem_t semThreadStart; 


typedef struct _threadParameters {
	bool *runStop;
	uint8_t nPanelNumber;
	int *pFileXlateMatrix;
	uint8_t *pFileBufferBaseAddress;
} ThreadParameters;

static int *pOffsetCheckTable;
static int nImageBytesNeeded;

// forward declarations - file internal routines
void initFileXlateMatrix(void);
void *ledStringWriteThread(void *vargp);

void initScreen(void)
{
	// init buffers
	initBuffers();
	
	// clear screen
	clearScreen();
	
	// run our file loader as test (working code yet?
	loadTestImage();
	
	nImageBytesNeeded = getImageSizeInBytes();
	pOffsetCheckTable = (void *)xmalloc(nImageBytesNeeded);

	
	// populate our fileBuffer indices
	initFileXlateMatrix();
	
	// init gpio
	initGPIO();
	
	// init display task
	//blinkLED();
	
	// scope our 0's
	//testBit0Send();
	
	// scope our 1's
	//testBit1Send();
	
	// scope our RESET's
	//testResetSend();

	// start display threads
	pthread_t taskPanelTop;
	pthread_t taskPanelMid;
	pthread_t taskPanelBot;
	
	sem_init(&semThreadStart, 0, 1); 
	
	uint8_t *pFileBufferBaseAddress = (uint8_t *)getBufferBaseAddress();

#define THREAD1_LIVE
//#define THREAD2_LIVE
//#define THREAD3_LIVE

#ifdef THREAD1_LIVE
	ThreadParameters panelTopParams = { &bThreadRun, 0, (int *)&fileXlateMatrix, pFileBufferBaseAddress };
	pthread_create(&taskPanelTop, NULL, ledStringWriteThread, (void*)&panelTopParams); 
#endif

#ifdef THREAD2_LIVE
	ThreadParameters panelMidParams = { &bThreadRun, 1, (int *)&fileXlateMatrix, pFileBufferBaseAddress };
	pthread_create(&taskPanelMid, NULL, ledStringWriteThread, (void*)&panelMidParams); 
#endif

#ifdef THREAD3_LIVE
	ThreadParameters panelBotParams = { &bThreadRun, 2, (int *)&fileXlateMatrix, pFileBufferBaseAddress };
	pthread_create(&taskPanelBot, NULL, ledStringWriteThread, (void*)&panelBotParams); 
#endif

	sleep(3 * 60);	// surely we're done in 3 minutes...
	
#ifdef THREAD1_LIVE
	pthread_join(taskPanelTop,NULL); // stop thread
#endif

#ifdef THREAD2_LIVE
	pthread_join(taskPanelMid,NULL); // stop thread
#endif

#ifdef THREAD3_LIVE
	pthread_join(taskPanelBot,NULL); // stop thread
#endif
	sem_destroy(&semThreadStart); 	// done with mutex
	
	// return GPIO to normal setup
	restoreGPIO();
}

void clearScreen(void)
{
	clearBuffers();
}

void initFileXlateMatrix(void)
{
	// panels are arranged in columns 8-pixels tall by 32 pixels wide
	//
	// column pixels are numbered bottom to top on right edge and every other column from there
	//  rest of columns are numbered top to bottom on the in-between columns.
	//
	//  Numbering:
	//
	// COL:00  01         28   29   30   31
	//   248  247	...  024  023  008  007
	//   249  246	...  025  022  009  006
	//   250  245	...  026  021  010  005
	//   251  244	...  027  020  011  004
	//   252  243	...  028  019  012  003
	//   253  242	...  029  018  013  002
	//   254  241	...  030  017  014  001
	//   255  240	...  031  016  015  000
	// 
	//  effectively we think of the panel as one long string of pixels (256 of them.)
	// 
	// the file buffer has an access routine to provide ptr to RGB tuple for X,Y in width,height space
	//  so call this routine to get pointer to location and also add in minor offset to R or G or B of color value too...
	//
	//  The file buffer is 32w x 24h so each panel is a third of the height but full width!
	//
	// get base address of our file buffer so we can calculate offsets
	uint8_t *pFileBufferBaseAddress = (uint8_t *)getBufferBaseAddress();
	
	for(int nPanelIndex = 0; nPanelIndex < NUMBER_OF_PANELS; nPanelIndex++) {	// [0-2] where 0 is top panel.
		for(int nByteOfColorIndex = 0; nByteOfColorIndex < (LEDS_PER_PANEL * BYTES_PER_LED); nByteOfColorIndex++) {	// [0-767]
			int nColorIndex = nByteOfColorIndex % BYTES_PER_LED;	// [0-2]
			int nPixelIndex = nByteOfColorIndex / BYTES_PER_LED;	// [0-255]
			int nColumnIndex = nByteOfColorIndex / (ROWS_PER_PANEL * BYTES_PER_LED);
			// invert file column value
			nColumnIndex = (COLUMNS_PER_PANEL - 1) - nColumnIndex;
			int nPanelColumnIndex = (COLUMNS_PER_PANEL - 1) - nColumnIndex;
			int nPanelRowIndex = (nColumnIndex & 1 == 1) ? nPixelIndex % ROWS_PER_PANEL : (ROWS_PER_PANEL - 1) - (nPixelIndex % ROWS_PER_PANEL);	// [0-7]
			int nRowIndex = (nPanelIndex * ROWS_PER_PANEL) + nPanelRowIndex;
			// invert file row value
			nRowIndex = ((ROWS_PER_PANEL * 3) - 1) - nRowIndex;
			printf("- RC={%d,%d} - pnl:%d, pnlRC={%d,%d} pxl:%d color:%d byte:%d\n",
				nRowIndex,
				nColumnIndex,
				nPanelIndex,
				nPanelRowIndex,
				nPanelColumnIndex,
				nPixelIndex,
				nColorIndex,
				nByteOfColorIndex);
				// at the beginning of each color do...
			struct _BMPColorValue *pCurrFilePixel;
			if(nColorIndex == 0) {
				pCurrFilePixel = getPixelAddressForRowColumn(nRowIndex, nColumnIndex);
			}
			uint8_t *pFileColorAddress;
			uint8_t *pFilePixelAddress = (uint8_t *)pCurrFilePixel;
			switch(nColorIndex) {
				case 0:	// string wants GREEN here
					pFileColorAddress = &pCurrFilePixel->green;
					break;
				case 1:	// string wants RED here
					pFileColorAddress = &pCurrFilePixel->red;
					break;
				case 2:	// string wants BLUE here
					pFileColorAddress = &pCurrFilePixel->blue;
					break;
				default:
					printf("- ERROR Bad color index (%d) NOT [0-2]\n", nColorIndex);
					break;
			}
			int nColorOffset = pFileColorAddress - pFilePixelAddress;
			int nFilePixelOffset = pFilePixelAddress - pFileBufferBaseAddress;
			int nOffsetValue = nFilePixelOffset + nColorOffset;
			fileXlateMatrix[nByteOfColorIndex] = nOffsetValue;
			printf("  -- OFFSET[%d] = pix:%d + clr:%d\n", nByteOfColorIndex, nFilePixelOffset, nColorOffset);
			
			// detect if offset used more than once. Should NEVER be!!
			if(nOffsetValue >= nImageBytesNeeded) {
				printf("\n- ERROR offset %d OUT OF RANGE: [0-%d]!",nOffsetValue, nImageBytesNeeded);
			}
			else {
				if(pOffsetCheckTable[nOffsetValue] != 0) {
					printf("\n- ERROR offset %d used more than once!",nOffsetValue);
				}
				else {
					pOffsetCheckTable[nOffsetValue] = 1;	// mark this as used
				}
			}
			printf("\n");	// blank line
		}
	}
}

// A normal C function that is executed as a thread  
// when its name is specified in pthread_create() 
void *ledStringWriteThread(void *vargp) 
{ 
    // type the parameter so we can use it
    ThreadParameters *parameters = (ThreadParameters *)vargp;
	// calc matrix for this panel
    int *pPanelFileXlateMatrix = &parameters->pFileXlateMatrix[parameters->nPanelNumber * (LEDS_PER_PANEL * BYTES_PER_LED)];
    // calc GPIO pin for this panel
    eLedStringPins threadsPin;
    switch(parameters->nPanelNumber) {
    	case 2: // bottom panel
    		threadsPin = LSP_BOTTOM;
    		break;
    	case 1: // middle panel
    		threadsPin = LSP_MIDDLE;
    		break;
    	default: // top panel
    		threadsPin = LSP_TOP;
    		break;
    }
    
    xmitReset(threadsPin);
    
	// run write loop forever

	//while(true) {
	    //wait 
	    printf("- THREAD panel-%d waiting\n", parameters->nPanelNumber); 
	    sem_wait(&semThreadStart); 
	    
	    // for each color send byte over GPIO bit 
	    for(int nColorIndex = 0; nColorIndex < (LEDS_PER_PANEL * BYTES_PER_LED); nColorIndex++) {
	    	int nFileBufferOffset = pPanelFileXlateMatrix[nColorIndex];
	    	uint8_t nColorValue = parameters->pFileBufferBaseAddress[nFileBufferOffset];
		if(nColorValue == 0xBC) {
			nColorValue = 0x80;
		}
	    	// we write MSB first to LED string!
	    	for(int nShiftValue = 7; nShiftValue >= 0; nShiftValue--) {
	    		if(((nColorValue >> nShiftValue) & 1) == 1) {
	    			xmitOne(threadsPin);
	    		}
	    		else {
	    			xmitZero(threadsPin);
	    		}
	    	}
	    }
	    
	    // now issue reset 
	    xmitReset(threadsPin);
	    
	    //signal done
	    printf("- THREAD panel-%d done\n", parameters->nPanelNumber); 
	    sem_post(&semThreadStart); 
	//}
    return NULL; 
} 
