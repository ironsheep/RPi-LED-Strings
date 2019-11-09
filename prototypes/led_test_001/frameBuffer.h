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

#ifndef FRAME_BUFFER_H
#define FRAME_BUFFER_H

#include <stdint.h> 

#define NUMBER_OF_PANELS 3
#define LEDS_PER_PANEL 256
#define BYTES_PER_LED 3

#define NUMBER_OF_BUFFERS 2

struct _LedPixel {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

//extern _LedPixel *pFrameBuffers; // [NUMBER_OF_BUFFERS][NUMBER_OF_PANELS][LEDS_PER_PANEL];

// call to initialize our frame buffer (allocate, set to black, etc.)
void initBuffers(void);

// reset buffers so LED Screen goes blank
void clearBuffers(void);

#endif /* FRAME_BUFFER_H */
