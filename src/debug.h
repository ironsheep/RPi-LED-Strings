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

#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

extern int bDebugEnabled;		/* --debug */
extern int bVerboseEnabled;		/* --verbose */

extern const char *pAppName;

// conditional message forms
void debugMessage(const char *format, ...);
void verboseMessage(const char *format, ...);

// unconditional message forms
void infoMessage(const char *format, ...);
void warningMessage(const char *format, ...);
void errorMessage(const char *format, ...);
void perrorMessage(const char *format, ...);    // decode-n-print errno, too!

// special formats
void hexDump (const char *desc, const void *addr, const int len);


#endif /* DEBUG_H */
