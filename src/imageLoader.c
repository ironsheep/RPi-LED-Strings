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

#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h> // for memxxx() and strxxx()


#include "imageLoader.h"
#include "frameBuffer.h"
#include "xmalloc.h"
#include "debug.h"

// NOTE the following must be packed: REF: https://stackoverflow.com/questions/8568432/is-gccs-attribute-packed-pragma-pack-unsafe

struct _BMPHeader {             // Total: 54 bytes
    uint16_t  type;             // Magic identifier: 0x4d42
    uint32_t  size;             // File size in bytes
    uint16_t  reserved1;        // Not used
    uint16_t  reserved2;        // Not used
    uint32_t  offset;           // Offset to image data in bytes from beginning of file (54 bytes)
    uint32_t  dib_header_size;  // DIB Header size in bytes (40 bytes)
    int32_t   width_px;         // Width of the image
    int32_t   height_px;        // Height of image
    uint16_t  num_planes;       // Number of color planes
    uint16_t  bits_per_pixel;   // Bits per pixel
    uint32_t  compression;      // Compression type
    uint32_t  image_size_bytes; // Image size in bytes
    int32_t   x_resolution_ppm; // Pixels per meter
    int32_t   y_resolution_ppm; // Pixels per meter
    uint32_t  num_colors;       // Number of colors
    uint32_t  important_colors; // Important colors
} __attribute__((packed));  // WARNING this MUST be PACKED!!!


// -----------------------
// file-local variables
//
static char sTestFileName[] = "8pxSquaresMarked.bmp";

static char *fileBuffer;

static int nRows;
static int nColumns;
static int nImageSizeInBytes;

static int *pOffsetCheckTable;
static int nImageBytesNeeded;

static int bSetupXlate = 0;

static int fileXlateMatrix[NUMBER_OF_PANELS * LEDS_PER_PANEL * BYTES_PER_LED];


// -----------------------
// forward declarations
//
void initLoadTranslation(void);
void initFileXlateMatrix(void);


// -----------------------
//  PUBLIC Methods
//
int getImageSizeInBytes(void)
{
    return nImageSizeInBytes;
}

struct _BMPColorValue *getBufferBaseAddress(void)
{
    return (struct _BMPColorValue *)fileBuffer;
}

struct _BMPColorValue *getPixelAddressForRowColumn(uint8_t nRow, uint8_t nColumn)
{
    if(nRow > nRows - 1) {
        printf("- ERROR bad nRow value [%d > %d]\n", nRow, nRows);
    }
    if(nColumn > nColumns - 1) {
        printf("- ERROR bad nColumn value [%d > %d]\n", nColumn, nColumns);
    }
    // Row is inverted in file...
    int nRowIndex = (nRows - 1) - nRow;
    // Column is normal in file...
    int nColumnIndex = nColumn;
    // now offset is simple (
    int nOffset = (nRowIndex * nColumns) + nColumnIndex;
    printf("- Offset=%d, RC=(%d,%d)\n", nOffset, nRow, nColumn);
    struct _BMPColorValue *rgbArray = (struct _BMPColorValue *)fileBuffer;

    return &rgbArray[nOffset];
}

void showPixelAtRC(uint8_t nRow, uint8_t nColumn)
{
    struct _BMPColorValue *pRGBvalue;
    pRGBvalue = getPixelAddressForRowColumn(nRow,nColumn);
    printf("- RC=%d,%d @ %p is RGB=(%2x,%2x,%2x)\n", nRow, nColumn, pRGBvalue, pRGBvalue->red, pRGBvalue->green, pRGBvalue->blue);
}

int fileExists(const char *fileSpec)
{
    int foundStatus = 1; // found

    struct stat st;
    if(stat(fileSpec, &st) != 0) {
        perrorMessage("stat() failure");
        foundStatus = 0; // not found
    }
    char *statYN = (foundStatus) ? "YES" : "no";
    debugMessage("fileExists(%s) -> %s", fileSpec, statYN);

    return foundStatus;
}

void loadTestImage(void)
{
    loadImageFromFile(sTestFileName, NULL);
}

struct _BMPColorValue *loadImageFromFile(const char *fileSpec, int *lengthOut)
{
    struct stat st;
    stat(fileSpec, &st);
    debugMessage("File %s is %d bytes", fileSpec, st.st_size);

    int counter;
    FILE *fpTestFile;
    struct _BMPHeader bmpHeaderData;
    debugMessage("File Header size=%u", sizeof(struct _BMPHeader));

    fpTestFile = fopen(fileSpec,"rb");
    if(!fpTestFile) {
        perrorMessage("fopen() failure");
        return NULL;
    }
    fread(&bmpHeaderData, sizeof(struct _BMPHeader), 1, fpTestFile);

    nRows = bmpHeaderData.height_px;
    nColumns = bmpHeaderData.width_px;

    int nImageBytesNeeded = bmpHeaderData.width_px * bmpHeaderData.height_px * 3;
    nImageSizeInBytes = nImageBytesNeeded;
    int nRowPadByteCount = (bmpHeaderData.height_px * 3) % 4;

    debugMessage("File %s: sz=%u, IMAGE h/w=(%d,%d) size=%u bytesNeeded=%d rowPad=%d", fileSpec, bmpHeaderData.size, bmpHeaderData.height_px, bmpHeaderData.width_px, bmpHeaderData.image_size_bytes, nImageBytesNeeded, nRowPadByteCount);

#ifdef SHOW_EXTRA_DEBUG
    hexDump("BMP Header", &bmpHeaderData, sizeof(struct _BMPHeader));

    printf("- Addr header struct: %p\n", &bmpHeaderData);
    printf("- Addr size: %p\n", &bmpHeaderData.size);
    printf("- Addr width_px: %p\n", &bmpHeaderData.width_px);
    printf("- Addr height_px: %p\n", &bmpHeaderData.height_px);
    printf("- Addr image_size_bytes: %p\n", &bmpHeaderData.image_size_bytes);
    uint8_t *pBuffer = (uint8_t *)&bmpHeaderData;
    printf("- Addr image data: %p\n", &pBuffer[bmpHeaderData.offset]);
#endif

    fileBuffer = xmalloc(nImageBytesNeeded);

    // move to start of image bytes
    fseek(fpTestFile, bmpHeaderData.offset, SEEK_SET);

    // read the image data into our buffer
    fread(fileBuffer, nImageBytesNeeded, 1, fpTestFile);

#ifdef SHOW_EXTRA_DEBUG
    hexDump("Image bytes", fileBuffer, nImageBytesNeeded);
#endif

    fclose(fpTestFile);

    if(lengthOut != NULL) {
        *lengthOut = getImageSizeInBytes();
    }

    if(!bSetupXlate) {
        initLoadTranslation();
        bSetupXlate = 1;    // we did this!
    }

    return getBufferBaseAddress();
}




// -----------------------
//  PRIVATE Methods
//
void initLoadTranslation(void)
{
	// fill translation buffer with not-set value
	memset(&fileXlateMatrix, 0xFF, sizeof(fileXlateMatrix));

	nImageBytesNeeded = getImageSizeInBytes();
	pOffsetCheckTable = (void *)xmalloc(nImageBytesNeeded);
	// fill offset-used buffer with not-set value
	memset(pOffsetCheckTable, 0x00, nImageBytesNeeded);

	// populate our fileBuffer indices
	initFileXlateMatrix();
}

void initFileXlateMatrix(void)
{
    debugMessage("initFileXlateMatrix() - ENTRY");
	// quick test of get routine (seeing issues)
	uint8_t *pCurrFilePixel = (uint8_t *)getPixelAddressForRowColumn(16, 4);
	printf("- TEST 16,4 = %p\n", pCurrFilePixel);
	pCurrFilePixel = (uint8_t *)getPixelAddressForRowColumn(8, 4);
	printf("- TEST 8,4 = %p\n", pCurrFilePixel);
	pCurrFilePixel = (uint8_t *)getPixelAddressForRowColumn(0, 4);
	printf("- TEST 0,4 = %p\n\n\n", pCurrFilePixel);


	// panels are arranged in columns 8-pixels tall by 32 pixels wide
	//
	// column pixels are numbered bottom to top on right edge and every other column from there
	//  rest of columns are numbered top to bottom on the in-between columns.
	//
	//  Numbering: //
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
		int nPanelOffsetIndex = (nPanelIndex * (COLUMNS_PER_PANEL * ROWS_PER_PANEL * BYTES_PER_LED));
		for(int nByteOfColorIndex = 0; nByteOfColorIndex < (LEDS_PER_PANEL * BYTES_PER_LED); nByteOfColorIndex++) {	// [0-767]
			int nColorIndex = nByteOfColorIndex % BYTES_PER_LED;	// [0-2]
			int nPixelIndex = nByteOfColorIndex / BYTES_PER_LED;	// [0-255]

			// FILE column index is inverted
			int nColumnIndex = nByteOfColorIndex / (ROWS_PER_PANEL * BYTES_PER_LED);	// [0-31]
			// - invert file column value
			nColumnIndex = (COLUMNS_PER_PANEL - 1) - nColumnIndex;

			// panel columns are inverted from file columns
			int nPanelColumnIndex = (COLUMNS_PER_PANEL - 1) - nColumnIndex;	// [0-31]
			// panel rows alternate being 0->7 or 7->0!
			int nPanelRowIndex = (nColumnIndex & 1 == 1) ? nPixelIndex % ROWS_PER_PANEL : (ROWS_PER_PANEL - 1) - (nPixelIndex % ROWS_PER_PANEL);	// [0-7]

			// FILE row index (rows within panel are inverted)
			int nRowIndex = (nPanelIndex * ROWS_PER_PANEL) + ((ROWS_PER_PANEL - 1) - nPanelRowIndex);

			struct _BMPColorValue *pCurrFilePixel;
			// at the beginning of each color do...
			if(nColorIndex == 0) {
				printf("\n----------------\n");
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
					printf("\n- ERROR Bad color index (%d) NOT [0-2]\n", nColorIndex);
					break;
			}
			int nColorOffset = pFileColorAddress - pFilePixelAddress;
			int nFilePixelOffset = pFilePixelAddress - pFileBufferBaseAddress;
			int nFileOffsetValue = nFilePixelOffset + nColorOffset;
			// load matrix location with file offset
			int nXlateOffset = nPanelOffsetIndex + nByteOfColorIndex;
			fileXlateMatrix[nXlateOffset] = nFileOffsetValue;
			printf("- File RC={%d,%d} - Panel[#%d, RC={%d,%d} px:%d color:%d byte:%d]",
				nRowIndex,
				nColumnIndex,
				nPanelIndex,
				nPanelRowIndex,
				nPanelColumnIndex,
				nPixelIndex,
				nColorIndex,
				nByteOfColorIndex);
			printf("  -- MATRIX[%d] = (%d); FILE [px:%d + clr:%d] @%p\n", nXlateOffset, nFileOffsetValue, nFilePixelOffset, nColorOffset, pCurrFilePixel);

			// detect if offset used more than once. Should NEVER be!!
			if(nFileOffsetValue >= nImageBytesNeeded) {
				printf("\n- ERROR file-offset %d OUT OF RANGE: [0-%d]!\n", nFileOffsetValue, nImageBytesNeeded);
			}
			else {
				if(pOffsetCheckTable[nFileOffsetValue] != 0) {
					printf("\n- ERROR file-offset %d used more than once!\n", nFileOffsetValue);
				}
				else {
					pOffsetCheckTable[nFileOffsetValue] = 1;	// mark this as used
				}
			}
		}
	}
	//  check to see that all offsets are used
	for(int nFileOffsetValue = 0; nFileOffsetValue < nImageBytesNeeded; nFileOffsetValue++) {	// [0-2] where 0 is top panel.
		// each of these bytes if set are now 1 vs. 0
		// if NOT let's warn!
		if(pOffsetCheckTable[nFileOffsetValue] == 0) {
			printf("- ERROR file-offset[%d] not used!\n",  nFileOffsetValue);
		}
	}
	// lastly check to see that all matrix locations are filled
	for(int nXlateOffset = 0; nXlateOffset < nImageBytesNeeded; nXlateOffset++) {	// [0-2] where 0 is top panel.
		// each of these bytes if set are now 1 vs. 0
		// if NOT let's warn!
		int nFileOffsetValue = fileXlateMatrix[nXlateOffset];
		if(nFileOffsetValue > nImageBytesNeeded || nFileOffsetValue < 0) {
			printf("- ERROR xlate[%d] not filled! -> has %d\n",  nXlateOffset, nFileOffsetValue);
		}
	}
    debugMessage("initFileXlateMatrix() - EXIT");
}
