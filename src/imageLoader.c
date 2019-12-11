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


#include "imageLoader.h"
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
} __attribute__((packed));	// WARNING this MUST be PACKED!!!

static char sTestFileName[] = "8pxSquaresMarked.bmp";

static char *fileBuffer;

static int nRows;
static int nColumns;
static int nImageSizeInBytes;

// forward declarations


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
	if(!fpTestFile)
	{
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
	
	return getBufferBaseAddress();
}


