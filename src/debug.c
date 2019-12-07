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
#include <stdarg.h>

#include "debug.h"

int bDebugEnabled;		/* --debug */
int bVerboseEnabled;		/* --verbose */

const char *pAppName = "{appName?}";

extern const char *pAppName;

// conditional message forms

// =============================================================================
//  Simple message printing functions
//
void debugMessage(const char *format, ...)
{
	va_list argp;
	
	if(bDebugEnabled) {
    	va_start(argp, fmt);
    	printf("%s(DBG): ", pAppName);
    	vprintf(format, argp);
    	printf("\n");
    	va_end(argp);
    }
}

void verboseMessage(const char *format, ...)
{
	va_list argp;
	
	if(bDebugEnabled) {
    	va_start(argp, fmt);
    	printf("%s:Verbose- ", pAppName);
    	vprintf(format, argp);
    	printf("\n");
    	va_end(argp);
    }
}

void infoMessage(const char *format, ...)
{
	va_list argp;
	
	va_start(argp, fmt);
	printf("%s:INFO- ", pAppName);
	vprintf(format, argp);
	printf("\n");
	va_end(argp);
}

void warningMessage(const char *format, ...)
{
	va_list argp;
	
	va_start(argp, fmt);
	printf("%s:WARNING- ", pAppName);
	vprintf(format, argp);
	printf("\n");
	va_end(argp);
}

void errorMessage(const char *format, ...)
{
	va_list argp;
	
	va_start(argp, fmt);
	fprintf(stderr, "%s:ERROR- ", pAppName);
	vfprintf(stderr, format, argp);
	fprintf(stderr, "\n");
	va_end(argp);
}

//
// =============================================================================


void hexDump (const char *desc, const void *addr, const int len) {
    int i;
    unsigned char buff[17];
    const unsigned char *pc = (const unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}
