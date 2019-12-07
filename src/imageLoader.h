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

#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

struct _BMPColorValue {
  uint8_t blue;
  uint8_t green;
  uint8_t red;
} __attribute__((packed));      // WARNING this MUST be PACKED!!!


void loadTestImage(void);

int getImageSizeInBytes(void);
struct _BMPColorValue *getBufferBaseAddress(void);
struct _BMPColorValue *getPixelAddressForRowColumn(uint8_t nRow, uint8_t nColumn);

#endif /* IMAGE_LOADER_H */
