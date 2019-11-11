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

#ifndef LED_GPIO_H
#define LED_GPIO_H

// our test hardware is mapped to the following GPIO pins
enum eLedStringPins { 
	LSP_TOP = 17,		// gpio.0 - bcm 17
	LSP_MIDDLE = 27,	// gpio.2 - bcm 27
	LSP_BOTTOM = 22,	// gpio.3 - bcm 22
};

void initGPIO(void);
void restoreGPIO(void);

void xmitOne(eLedStringPins gpioPin);
void xmitZero(eLedStringPins gpioPin);
void xmitReset(eLedStringPins gpioPin);


// test routines so we can scope things
void blinkLED(void);
void testBit0Send(void);
void testBit1Send(void);
void testResetSend(void);

#endif /* LED_GPIO_H */
