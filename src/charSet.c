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

┌───────────────────────────────────┬─────────────────────────────────────────┬───────────────┐
│ 5x7_CharSet v1.0                  │ (C)2013 Stephen Moraco, KZ0Q            │ 02 Jan 2013   │
├───────────────────────────────────┴─────────────────────────────────────────┴───────────────┤
│ PREMISE: an object which looks up ROM char addresses to bit definitions for 5x7 chars       │
│  given the ASCII value for the character.                                                   │
│                                                                                             │
│ FEATURES:                                                                                   │
│  - Returns address of [] char when out-of-range                                             │
│  - Supports selection of alternate Micr-like number-set                                     │
│  - Supports query to see if alternate Micr-like number-set is selected                      │
│                                                                                             │
│ LAYOUT OF CHARS:                                                                            │
│                                       (Example Encoding)                                    │
│  Byte Offset: 0 1 2 3 4                   0 1 2 3 4                                         │
│              ┌─┬─┬─┬─┬─┐                 ┌─┬─┬─┬─┬─┐                                        │
│              │ │ │ │ │ │0                │ │ │ │x│ │0                                       │
│              ├─┼─┼─┼─┼─┤                 ├─┼─┼─┼─┼─┤                                        │
│              │ │ │ │ │ │1                │ │ │x│x│ │1                                       │
│              ├─┼─┼─┼─┼─┤                 ├─┼─┼─┼─┼─┤                                        │
│              │ │ │ │ │ │2                │ │x│ │x│ │2                                       │
│              ├─┼─┼─┼─┼─┤                 ├─┼─┼─┼─┼─┤                                        │
│              │ │ │ │ │ │3                │x│ │ │x│ │3                                       │
│              ├─┼─┼─┼─┼─┤                 ├─┼─┼─┼─┼─┤                                        │
│              │ │ │ │ │ │4                │x│x│x│x│x│4                                       │
│              ├─┼─┼─┼─┼─┤                 ├─┼─┼─┼─┼─┤                                        │
│              │ │ │ │ │ │5                │ │ │ │x│ │5                                       │
│              ├─┼─┼─┼─┼─┤                 ├─┼─┼─┼─┼─┤                                        │
│              │ │ │ │ │ │6                │ │ │ │x│ │6                                       │
│              └─┴─┴─┴─┴─┘                 └─┴─┴─┴─┴─┘                                        │
│                                                                                      │
│               │         Bit number!       │ │ │ │ │                                         │
│               │                          $18│ │ │ │                                         │
│               First Byte of 5              $14│ │ │                                         │
│                                              $12│ │                                         │
│                                                $7f│                                         │
│                                                  $10                                        │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│ TYPICAL USE                                                                                 │
│   Include this object...                                                                    │
│   Ask it for address of the 5-bytes of 5x7 char def'n for given ASCII code [20-7f]          │
│   Optional: toggle use of Micr-like numeric char set on/off                                 │
│                                                                                             │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│ Revision History                                                                            │
│                                                                                             │
│ v1.0  Initial Draft                                                                         │
│                                                                                             │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
*/

#include "charSet.h"


static int s_bUsingMicrDigits = 0;

void useMicrNumbers(int bEnable)
{
    // Enable/Disable use of special Micr-like digits
    s_bUsingMicrDigits = bEnable;
}

int isUsingMicrNumbers(void)
{
    // Return T/F where T means we are using Micr-like digits
    return s_bUsingMicrDigits != 0;
}

static uint8_t s_nRomChars[] = {

// Micr-like is really hand drawn... from: http://www.parallax.com/Portals/0/Downloads/docs/cols/nv/vol3/col/nv77.pdf
// MicrNbrs
    0x7f, 0x79, 0x41, 0x41, 0x7f,        // 30 0
    0x00, 0x78, 0x7f, 0x00, 0x00,        // 31 1
    0x79, 0x79, 0x49, 0x49, 0x4f,        // 32 2
    0x49, 0x49, 0x49, 0x7f, 0x78,        // 33 3
    0x1f, 0x1f, 0x10, 0x10, 0x78,        // 34 4
    0x4f, 0x4f, 0x49, 0x49, 0x79,        // 35 5
    0x7f, 0x79, 0x48, 0x48, 0x78,        // 36 6
    0x01, 0x03, 0x01, 0x7b, 0x7f,        // 37 7
    0x78, 0x4f, 0x49, 0x7f, 0x78,        // 38 8
    0x0f, 0x09, 0x09, 0x79, 0x7f,        // 39 9

// NoSuchCharBOX
    0x3e, 0x22, 0x22, 0x22, 0x3e,        // []

// Standard character map
// Ascii20_7f
    0x00, 0x00, 0x00, 0x00, 0x00,        // 20
    0x00, 0x00, 0x5f, 0x00, 0x00,        // 21 !
    0x00, 0x07, 0x00, 0x07, 0x00,        // 22 "
    0x14, 0x7f, 0x14, 0x7f, 0x14,        // 23 #
    0x24, 0x2a, 0x7f, 0x2a, 0x12,        // 24 0x
    0x23, 0x13, 0x08, 0x64, 0x62,        // 25 %
    0x36, 0x49, 0x55, 0x22, 0x50,        // 26 &
    0x00, 0x05, 0x03, 0x00, 0x00,        // 27 //
    0x00, 0x1c, 0x22, 0x41, 0x00,        // 28 (
    0x00, 0x41, 0x22, 0x1c, 0x00,        // 29 )
    0x14, 0x08, 0x3e, 0x08, 0x14,        // 2a *
    0x08, 0x08, 0x3e, 0x08, 0x08,        // 2b +
    0x00, 0x50, 0x30, 0x00, 0x00,        // 2c ,
    0x08, 0x08, 0x08, 0x08, 0x08,        // 2d -
    0x00, 0x60, 0x60, 0x00, 0x00,        // 2e .
    0x20, 0x10, 0x08, 0x04, 0x02,        // 2f /
    0x3e, 0x51, 0x49, 0x45, 0x3e,        // 30 0
    0x00, 0x42, 0x7f, 0x40, 0x00,        // 31 1
    0x42, 0x61, 0x51, 0x49, 0x46,        // 32 2
    0x21, 0x41, 0x45, 0x4b, 0x31,        // 33 3
    0x18, 0x14, 0x12, 0x7f, 0x10,        // 34 4
    0x27, 0x45, 0x45, 0x45, 0x39,        // 35 5
    0x3c, 0x4a, 0x49, 0x49, 0x30,        // 36 6
    0x01, 0x71, 0x09, 0x05, 0x03,        // 37 7
    0x36, 0x49, 0x49, 0x49, 0x36,        // 38 8
    0x06, 0x49, 0x49, 0x29, 0x1e,        // 39 9
    0x00, 0x36, 0x36, 0x00, 0x00,        // 3a :
    0x00, 0x56, 0x36, 0x00, 0x00,        // 3b ;
    0x08, 0x14, 0x22, 0x41, 0x00,        // 3c <
    0x14, 0x14, 0x14, 0x14, 0x14,        // 3d =
    0x00, 0x41, 0x22, 0x14, 0x08,        // 3e >
    0x02, 0x01, 0x51, 0x09, 0x06,        // 3f ?
    0x32, 0x49, 0x79, 0x41, 0x3e,        // 40 @
    0x7e, 0x11, 0x11, 0x11, 0x7e,        // 41 A
    0x7f, 0x49, 0x49, 0x49, 0x36,        // 42 B
    0x3e, 0x41, 0x41, 0x41, 0x22,        // 43 C
    0x7f, 0x41, 0x41, 0x22, 0x1c,        // 44 D
    0x7f, 0x49, 0x49, 0x49, 0x41,        // 45 E
    0x7f, 0x09, 0x09, 0x09, 0x01,        // 46 F
    0x3e, 0x41, 0x49, 0x49, 0x7a,        // 47 G
    0x7f, 0x08, 0x08, 0x08, 0x7f,        // 48 H
    0x00, 0x41, 0x7f, 0x41, 0x00,        // 49 I
    0x20, 0x40, 0x41, 0x3f, 0x01,        // 4a J
    0x7f, 0x08, 0x14, 0x22, 0x41,        // 4b K
    0x7f, 0x40, 0x40, 0x40, 0x40,        // 4c L
    0x7f, 0x02, 0x0c, 0x02, 0x7f,        // 4d M
    0x7f, 0x04, 0x08, 0x10, 0x7f,        // 4e N
    0x3e, 0x41, 0x41, 0x41, 0x3e,        // 4f O
    0x7f, 0x09, 0x09, 0x09, 0x06,        // 50 P
    0x3e, 0x41, 0x51, 0x21, 0x5e,        // 51 Q
    0x7f, 0x09, 0x19, 0x29, 0x46,        // 52 R
    0x46, 0x49, 0x49, 0x49, 0x31,        // 53 S
    0x01, 0x01, 0x7f, 0x01, 0x01,        // 54 T
    0x3f, 0x40, 0x40, 0x40, 0x3f,        // 55 U
    0x1f, 0x20, 0x40, 0x20, 0x1f,        // 56 V
    0x3f, 0x40, 0x38, 0x40, 0x3f,        // 57 W
    0x63, 0x14, 0x08, 0x14, 0x63,        // 58 X
    0x07, 0x08, 0x70, 0x08, 0x07,        // 59 Y
    0x61, 0x51, 0x49, 0x45, 0x43,        // 5a Z
    0x00, 0x7f, 0x41, 0x41, 0x00,        // 5b [
    0x02, 0x04, 0x08, 0x10, 0x20,        // 5c ¥
    0x00, 0x41, 0x41, 0x7f, 0x00,        // 5d ]
    0x04, 0x02, 0x01, 0x02, 0x04,        // 5e ^
    0x40, 0x40, 0x40, 0x40, 0x40,        // 5f _
    0x00, 0x01, 0x02, 0x04, 0x00,        // 60 `
    0x20, 0x54, 0x54, 0x54, 0x78,        // 61 a
    0x7f, 0x48, 0x44, 0x44, 0x38,        // 62 b
    0x38, 0x44, 0x44, 0x44, 0x20,        // 63 c
    0x38, 0x44, 0x44, 0x48, 0x7f,        // 64 d
    0x38, 0x54, 0x54, 0x54, 0x18,        // 65 e
    0x08, 0x7e, 0x09, 0x01, 0x02,        // 66 f
    0x0c, 0x52, 0x52, 0x52, 0x3e,        // 67 g
    0x7f, 0x08, 0x04, 0x04, 0x78,        // 68 h
    0x00, 0x44, 0x7d, 0x40, 0x00,        // 69 i
    0x20, 0x40, 0x44, 0x3d, 0x00,        // 6a j
    0x7f, 0x10, 0x28, 0x44, 0x00,        // 6b k
    0x00, 0x41, 0x7f, 0x40, 0x00,        // 6c l
    0x7c, 0x04, 0x18, 0x04, 0x78,        // 6d m
    0x7c, 0x08, 0x04, 0x04, 0x78,        // 6e n
    0x38, 0x44, 0x44, 0x44, 0x38,        // 6f o
    0x7c, 0x14, 0x14, 0x14, 0x08,        // 70 p
    0x08, 0x14, 0x14, 0x18, 0x7c,        // 71 q
    0x7c, 0x08, 0x04, 0x04, 0x08,        // 72 r
    0x48, 0x54, 0x54, 0x54, 0x20,        // 73 s
    0x04, 0x3f, 0x44, 0x40, 0x20,        // 74 t
    0x3c, 0x40, 0x40, 0x20, 0x7c,        // 75 u
    0x1c, 0x20, 0x40, 0x20, 0x1c,        // 76 v
    0x3c, 0x40, 0x30, 0x40, 0x3c,        // 77 w
    0x44, 0x28, 0x10, 0x28, 0x44,        // 78 x
    0x0c, 0x50, 0x50, 0x50, 0x3c,        // 79 y
    0x44, 0x64, 0x54, 0x4c, 0x44,        // 7a z
    0x00, 0x08, 0x36, 0x41, 0x00,        // 7b {
    0x00, 0x00, 0x7f, 0x00, 0x00,        // 7c |
    0x00, 0x41, 0x36, 0x08, 0x00,        // 7d }
    0x10, 0x08, 0x08, 0x10, 0x08,        // 7e ~
    0x78, 0x46, 0x41, 0x46, 0x78,        // 7f
};

#define BYTES_PER_CHAR 5
#define OFFSET_MICR_DIGITS 0
#define OFFSET_NO_SUCH_CHAR_BOX (10 * BYTES_PER_CHAR)
#define OFFSET_CHARS_20_7F (OFFSET_NO_SUCH_CHAR_BOX + (1 * BYTES_PER_CHAR))

const uint8_t *getCharBitsAddr(uint8_t cCharacter)
{
    // default to invalid char symbol
    uint8_t *desiredAddr = &s_nRomChars[OFFSET_NO_SUCH_CHAR_BOX];

    if(cCharacter >= 0x30 && cCharacter <= 0x39 && s_bUsingMicrDigits != 0) {
        // Return addr of bits for uint8_t[0x30-0x39] (ptr to 5-bytes) MICR Digits
        uint16_t charOffset = (cCharacter - 0x30) * BYTES_PER_CHAR;
        desiredAddr = &s_nRomChars[OFFSET_MICR_DIGITS + charOffset];
    }
    else if(cCharacter >= 0x20 && cCharacter <= 0x7f) {
        // Return addr of bits for uint8_t[0x20-0x7f] (ptr to 5-bytes)
        uint16_t charOffset = (cCharacter - 0x20) * BYTES_PER_CHAR;
        desiredAddr = &s_nRomChars[OFFSET_CHARS_20_7F + charOffset];
    }
    return desiredAddr;
}

