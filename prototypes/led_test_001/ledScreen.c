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

#include "ledScreen.h"
#include "frameBuffer.h"
#include "ledGPIO.h"

void initScreen(void)
{
	// init buffers
	initBuffers();
	
	// init gpio
	initGPIO();
	
	// clear screen
	clearScreen();
	
	// init display task
	
}

void clearScreen(void)
{
	clearBuffers();
}
