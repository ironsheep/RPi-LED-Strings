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

#include <sys/stat.h>
#include <stdtypes.h>
#include <stdio.h> 


#include "imageLoader.h"
#include "xmalloc.h"

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
};

static char sTestFileName[] = "8pxSquaresLines.bmp";

static char *fileBuffer;

void loadTestImage(void) 
{
	struct stat st;
	stat(sTestFileName, &st);
	size_t nFileSize = st.st_size;
	
	printf("File %s is %d bytes\n", sTestFileName, nFileSize);
	fileBuffer = xmalloc(nFileSize);

	int counter;
	FILE *fpTestFile;
	struct _BMPHeader bmpHeaderData;
	
	fpTestFile = fopen(sTestFileName,"rb");
	if(!fpTestFile)
	{
		printf("ERROR: Unable to open file!\n");
		return;
	}
	fread(&bmpHeaderData, sizeof(struct _BMPHeader), 1, fpTestFile);
	printf("File %s: sz=%d, h/w=(%d,%d)\n", sTestFileName, bmpHeaderData.size, bmpHeaderData.height_px, bmpHeaderData.width_px);
	fclose(fpTestFile);
	
}
