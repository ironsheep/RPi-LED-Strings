/*
matrix -    interactive LED Matrix console

Copyright (C) 2019 Stephen M    Moraco

This    program is free software; you can redistribute it and/or modify
it under    the terms of the GNU General Public License as published by
the Free    Software Foundation; either version 2, or (at your option)
any later version.

This    program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A    PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received    a copy of the GNU General Public License
along with this program;    if not, write to the Free Software Foundation,
Inc., 59    Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include <stdio.h>
#include <string.h>

#include "frameBuffer.h"
#include "xmalloc.h"
#include "debug.h"
#include "charSet.h"

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

    //debugMessage("allocBuffers(%d) - ENTRY", nDesiredBuffers);
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
                nNumberAllocatedBuffers++;
            }
        }
        pFrameBufferAr[nDesiredBuffers] = NULL; // place trailing NULL at end of list
    }
    //debugMessage("allocBuffers() - EXIT");
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

uint16_t frameBufferSizeInBytes(void)
{
    return nLenFrameBuffer;
}

struct _LedPixel *ptrBuffer(uint8_t nBufferNumber)
{
    struct _LedPixel *desiredAddr = NULL;

    if(nBufferNumber > 0 && nBufferNumber <= nNumberAllocatedBuffers) {
        desiredAddr = (struct _LedPixel *)pFrameBufferAr[nBufferNumber - 1];
    }
    else {
        if(nBufferNumber < 1 || nBufferNumber > MAX_BUFFER_POINTERS) {
            warningMessage("buffer %d out-of-range: [1-%d]", nBufferNumber, MAX_BUFFER_POINTERS);
        }
        else {
            warningMessage("buffer %d NOT yet Allocated. Use 'buffers %d' to allocate it", nBufferNumber, nBufferNumber);
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

void fillBufferWithColorRGB(uint8_t nBufferNumber, uint32_t nColorRGB)
{
    struct _LedPixel *pSelectedBuffer = ptrBuffer(nBufferNumber);
    if(pSelectedBuffer != NULL) {
        int nMaxLEDs = maxLedsInBuffer();
        uint8_t red = (nColorRGB >> 16) & 0xff;
        uint8_t green = (nColorRGB >> 8) & 0xff;
        uint8_t blue = (nColorRGB >> 0) & 0xff;
        for(int nLedIdx = 0; nLedIdx < nMaxLEDs; nLedIdx++) {
            pSelectedBuffer[nLedIdx].red = red;
            pSelectedBuffer[nLedIdx].green = green;
            pSelectedBuffer[nLedIdx].blue = blue;
        }
    }
    else {
        errorMessage("fillBufferWithColorRGB() No Buffer at #%d", nBufferNumber);
    }
}

void fillBufferPanelWithColorRGB(uint8_t nBufferNumber, uint8_t nPanelNumber, uint32_t nColorRGB)
{
    // We handle panel spec of 12 and 23 !!
    int nLedsPerPanel = LEDS_PER_PANEL;
    if(nPanelNumber == 12) {
        // our "panel" is BOTH panels 1 & 2
        nPanelNumber = 1;
        nLedsPerPanel = 2 * LEDS_PER_PANEL;
    }
    else if(nPanelNumber == 23) {
        // our "panel" is BOTH panels 2 & 3
        nPanelNumber = 2;
        nLedsPerPanel = 2 * LEDS_PER_PANEL;
    }
    struct _LedPixel *pSelectedBuffer = ptrBuffer(nBufferNumber);
    if(pSelectedBuffer != NULL && nPanelNumber >= 1 && nPanelNumber <= NUMBER_OF_PANELS) {
       	uint16_t nOffsetToPanel = (nPanelNumber - 1) * nLedsPerPanel;
        struct _LedPixel *pSelectedBufferPanel = &pSelectedBuffer[nOffsetToPanel];
        uint8_t red = (nColorRGB >> 16) & 0xff;
        uint8_t green = (nColorRGB >> 8) & 0xff;
        uint8_t blue = (nColorRGB >> 0) & 0xff;
    	for(int nLedIdx = 0; nLedIdx < LEDS_PER_PANEL; nLedIdx++) {
            pSelectedBufferPanel[nLedIdx].red = red;
            pSelectedBufferPanel[nLedIdx].green = green;
            pSelectedBufferPanel[nLedIdx].blue = blue;
    	}
    }
    else {
        errorMessage("fillBufferPanelWithColorRGB() No Buffer at #%d, panel-#%d", nBufferNumber, nPanelNumber);
    }
}

void setBufferLEDColor(uint8_t nBufferNumber, uint32_t nColorRGB, uint8_t locX, uint8_t locY)
{
    struct _LedPixel *pSelectedBuffer = ptrBuffer(nBufferNumber);
    if(pSelectedBuffer != NULL) {
        // do we have bottom to top numbered column
        int bGoesUpPanelY = (locX & 0x01) == 1;
        uint8_t nPanelIdx = (locY / 8);
        uint8_t nPanelY = (locY % 8);
        uint16_t nColumnLEDidx = ((32 - 1) - locX) * 8;
        uint16_t nOffsetToPanel = nPanelIdx * LEDS_PER_PANEL;
        uint16_t nLEDIdx = (bGoesUpPanelY == 0) ? nColumnLEDidx + nPanelY : nColumnLEDidx + (7 - nPanelY);
        nLEDIdx += nOffsetToPanel;

        uint8_t red = (nColorRGB >> 16) & 0xff;
        uint8_t green = (nColorRGB >> 8) & 0xff;
        uint8_t blue = (nColorRGB >> 0) & 0xff;
        pSelectedBuffer[nLEDIdx].red = red;
        pSelectedBuffer[nLEDIdx].green = green;
        pSelectedBuffer[nLEDIdx].blue = blue;
    }
    else {
        errorMessage("setBufferLEDColor() No Buffer at #%d", nBufferNumber);
    }
}

void drawSquareInBuffer(uint8_t nBufferNumber, uint8_t locX, uint8_t locY, uint8_t nPanelNumber, uint8_t nWidth, uint8_t nHeight, uint8_t nLineWidth, uint32_t nLineColor)
{
    // We handle panel spec of 12 and 23 !!
    int nRowsPerPanel = 8;
    if(nPanelNumber == 12) {
        // our "panel" is BOTH panels 1 & 2
        nPanelNumber = 1;
        nRowsPerPanel = 2 * 8;
    }
    else if(nPanelNumber == 23) {
        // our "panel" is BOTH panels 2 & 3
        nPanelNumber = 2;
        nRowsPerPanel = 2 * 8;
    }

    if(nPanelNumber != 0) {
       locY = (nPanelNumber - 1) * 8;
       nHeight = nRowsPerPanel;
    }

    moveToInBuffer(nBufferNumber, locX, locY);

    lineToInBuffer(nBufferNumber, locX + nWidth - 1, locY, nLineWidth, nLineColor, nRowsPerPanel);
    lineToInBuffer(nBufferNumber, locX + nWidth - 1, locY + nHeight -1, nLineWidth, nLineColor, nRowsPerPanel);
    lineToInBuffer(nBufferNumber, locX, locY + nHeight -1, nLineWidth, nLineColor, nRowsPerPanel);
    lineToInBuffer(nBufferNumber, locX, locY, nLineWidth, nLineColor, nRowsPerPanel);
}

static int nPenX;
static int nPenY;

void moveToInBuffer(uint8_t nBufferNumber, uint8_t locX, uint8_t locY)
{
    debugMessage("moveTo() bfr #%d rc=(%d, %d)", nBufferNumber, locX, locY);
    nPenX = locX;
    nPenY = locY;
}

#define MIN(a,b) ((a < b) ? a : b)
#define MAX(a,b) ((a > b) ? a : b)

void lineToInBuffer(uint8_t nBufferNumber, uint8_t locX, uint8_t locY, uint8_t nLineWidth, uint32_t nLineColor, uint8_t nAreaHeight)
{
    debugMessage("lineTo() bfr #%d fmRC=(%d, %d), toRC=(%d, %d), w=%d, c=0x%.06X", nBufferNumber, nPenX, nPenY, locX, locY, nLineWidth, nLineColor);
    int nLineWidthAdjust = (nLineWidth - 1);
    int bIsHorzOrVertLine = (nPenX == locX) || (nPenY == locY);
    if(bIsHorzOrVertLine) {
        if(nPenX == locX) {
            // draw vertical line
            int nMinIdxY = MIN(nPenY, locY);
            int nMaxIdxY = MAX(nPenY, locY);
            // adjust if nLineWidth offscreen to right
            if((nPenX + nLineWidthAdjust) > 31) {
                nPenX -= nLineWidthAdjust;
            }
            for(int yIdx = nMinIdxY; yIdx <= nMaxIdxY; yIdx++) {
                setBufferLEDColor(nBufferNumber, nLineColor, nPenX, yIdx);
                if(nLineWidth > 1) {
                    for(int nLineCt = 0; nLineCt < nLineWidth - 1; nLineCt++) {
                        setBufferLEDColor(nBufferNumber, nLineColor, nPenX + nLineCt + 1, yIdx);
                    }
                }
            }
            nPenY = locY;
        }
        else {
            // draw horizontal line
            int nMinIdxX = MIN(nPenX, locX);
            int nMaxIdxX = MAX(nPenX, locX);
            // adjust if nLineWidth offscreen to bottom
            if((nPenY + nLineWidthAdjust) > nAreaHeight - 1) {
                nPenY -= nLineWidthAdjust;
            }
            for(int xIdx = nMinIdxX; xIdx <= nMaxIdxX; xIdx++) {
                setBufferLEDColor(nBufferNumber, nLineColor, xIdx, nPenY);
                if(nLineWidth > 1) {
                    for(int nLineCt = 0; nLineCt < nLineWidth - 1; nLineCt++) {
                        setBufferLEDColor(nBufferNumber, nLineColor, xIdx, nPenY + nLineCt + 1);
                    }
                }
            }
            nPenX = locX;
       }
    }
    else {
        warningMessage("- lineToInBuffer() sloped line NOT YET implemented, draw skipped.");
    }
}

void writeStringToBufferWithColorRGB(uint8_t nBufferNumber, const char *cString, uint32_t nColorRGB)
{
    int strLen = strlen(cString);
    char *rwCString = (char *)cString;
    if(*cString == '"' && cString[strLen-1] == '"' && strLen > 2) {
        cString++;
    	rwCString[strLen-1] = 0x00;
    	strLen -= 2;
    }

    writeStringToBufferPanelWithColorRGB(nBufferNumber, &cString[0], 1, nColorRGB);
    const char *pWord2 = &cString[5];
    while(*pWord2 == ' ') {
	    pWord2++;
    }
    if(*pWord2 != 0x00) {
        writeStringToBufferPanelWithColorRGB(nBufferNumber, pWord2, 2, nColorRGB);
    	if(strlen(pWord2) >= 5) {
            const char *pWord3 = &pWord2[5];
            while(*pWord3 == ' ') {
        	    pWord3++;
            }
            if(*pWord3 != 0x00) {
                writeStringToBufferPanelWithColorRGB(nBufferNumber, pWord3, 3, nColorRGB);
        	}
        }
    }
}

void writeStringToBufferPanelWithColorRGB(uint8_t nBufferNumber, const char *cString, uint8_t nPanelNumber, uint32_t nColorRGB)
{
    // we support panel spec of 12 and 23 !! (these place us into middle of panel pair! - centered within the two panels)
    float fPanelNumber = nPanelNumber;
    int bCenterString = 0;
    if(nPanelNumber == 12) {
        fPanelNumber = 1.5;
        bCenterString = 1;
    }
    else if(nPanelNumber == 23) {
        fPanelNumber = 2.5;
        bCenterString = 1;
    }
    int locY = ((fPanelNumber - 1) * ROWS_PER_PANEL);
    int strLenInPx = (strlen(cString) * 6); // chars * nbrPx/Char
    int locX = 1;
    if(bCenterString && strLenInPx < 30) {
        locX = (32 - strLenInPx) / 2;
    }
    for(int nCharIdx = 0; nCharIdx < strlen(cString); nCharIdx++) {
        locX = setCharToBuffer(nBufferNumber, cString[nCharIdx], locX, locY,nColorRGB);
        locX += 1; // add gap between chars
        if(locX + 5 > 31) {
            break;
        }
    }
}

int setCharToBuffer(uint8_t nBufferNumber, char cChar, uint8_t locX, uint8_t locY, uint32_t nColorRGB)
{
    // place 5 bytes of LED on/off info into buffer starting at top left corner X,Y
    // returns next X addres after;
    uint8_t nextLocX;
    const uint8_t *charRom5Bytes = getCharBitsAddr(cChar);
    for(int nRomIdx = 0; nRomIdx < 5; nRomIdx++) {
        nextLocX = locX + nRomIdx;
        uint8_t nRomByte = charRom5Bytes[nRomIdx];
        // now place LED bits ON with color if bit is set (bit0 is top, bit6 is bottom)
        for(int nBitIdx = 0; nBitIdx < 7; nBitIdx++) {
            if((nRomByte & (1 << nBitIdx)) != 0) {
                setBufferLEDColor(nBufferNumber, nColorRGB, nextLocX, locY + nBitIdx);
            }
            else {
                setBufferLEDColor(nBufferNumber, 0x000000, nextLocX, locY + nBitIdx);
            }
        }
    }
    nextLocX += 1;
    return nextLocX;
}
