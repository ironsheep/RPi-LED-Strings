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

#ifndef CLOCK_DISPLAY_H
#define CLOCK_DISPLAY_H

#include <stdint.h>

typedef enum _eClockFaceTypes {
    CFT_Unknown = 0,
    CFT_DIGITAL,
    CFT_BINARY,
} eClockFaceTypes;

void runClock(eClockFaceTypes clockType, utint32_t nFaceColor);
void stopClock(void);
int isClockRunning(void);

#endif /* CLOCK_DISPLAY_H */
