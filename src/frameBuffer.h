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

#ifndef FRAME_BUFFER_H
#define FRAME_BUFFER_H

#include <stdint.h> 

#define BYTES_PER_LED 3
#define LEDS_PER_PANEL 256
#define NUMBER_OF_PANELS 3

#define ROWS_PER_PANEL 8
#define COLUMNS_PER_PANEL 32

struct _LedPixel {
	uint8_t green;	// sent first to string in order msb to lsb!
	uint8_t red;
	uint8_t blue;	// sent last to string
} __attribute__((packed));	// WARNING this MUST be PACKED!!!

//extern _LedPixel *pFrameBuffers; // [NUMBER_OF_BUFFERS][NUMBER_OF_PANELS][LEDS_PER_PANEL];

// call to initialize our frame buffer (allocate, set to black, etc.)
void initBuffers(void);

// reset all buffers so LED Screen goes blank when written
void clearBuffers(void);

// alloc requested number of buffers (zero filled)
int allocBuffers(int nDesiredBuffers);

// object counts
uint8_t numberBuffers(void);
uint8_t numberPanels(void);
uint16_t maxLedsInBuffer(void);
uint16_t maxLedsInPanel(void);

// getting references to objects
struct _LedPixel *ptrBuffer(uint8_t nBuffer); 
struct _LedPixel *ptrPanel(struct _LedPixel *pBuffer, uint8_t nPanel);

#endif /* FRAME_BUFFER_H */
