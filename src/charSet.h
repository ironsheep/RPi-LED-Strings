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

#ifndef CHAR_SET_H
#define CHAR_SET_H

#include <stdint.h>

void useMicrNumbers(int bEnable); // Enable/Disable use of special Micr-like digits
int isUsingMicrNumbers(void);     // Return T/F where T means we are using Micr-like digits
const int8_t *getCharBitsAddr(char cCharacter); // Return addr of bits for char[0x20-0x7f] (ptr to 5-bytes)

#endif /* CHAR_SET_H */
