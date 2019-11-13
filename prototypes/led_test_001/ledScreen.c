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

#include "ledScreen.h"
#include "frameBuffer.h"	// screen sizes too
#include "imageLoader.h"
#include "ledGPIO.h"

static int fileXlateMatrix[NUMBER_OF_PANELS * LEDS_PER_PANEL * BYTES_PER_LED];

// forward declarations - file internal routines
void initFileXlateMatrix(void);


void initScreen(void)
{
	// init buffers
	initBuffers();
	
	// clear screen
	clearScreen();
	
	// run our file loader as test (working code yet?
	loadTestImage();
	
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
	uint8_t *pFileBufferBase = (uint8_t *)getBufferBaseAddress();
	
	for(int nPanelIndex = 0; nPanelIndex < NUMBER_OF_PANELS; nPanelIndex++) {	// [0-2] where 0 is top panel.
		for(int nByteOfColorIndex = 0; nByteOfColorIndex < (LEDS_PER_PANEL * BYTES_PER_LED); nByteOfColorIndex++) {	// [0-767]
			int nColorIndex = nByteOfColorIndex % BYTES_PER_LED;	// [0-2]
			int nPixelIndex = nByteOfColorIndex / BYTES_PER_LED;	// [0-255]
			int nColumnIndex = nByteOfColorIndex / (ROWS_PER_PANEL * BYTES_PER_LED);
			int nPanelColumnIndex = (COLUMNS_PER_PANEL - 1) - nColumnIndex;
			int nPanelRowIndex = (nColumnIndex & 1 == 1) ? nPixelIndex % ROWS_PER_PANEL : (ROWS_PER_PANEL - 1) - (nPixelIndex % ROWS_PER_PANEL);	// [0-7]
			int nRowIndex = (nPanelIndex * ROWS_PER_PANEL) + nPanelRowIndex;
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
			fileXlateMatrix[nByteOfColorIndex] = nFilePixelOffset + nColorOffset;
			printf("- OFFSET[%d] = pix:%d + clr:%d\n", nByteOfColorIndex, nFilePixelOffset, nColorOffset);
		}
	}
}
