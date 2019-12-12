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

#include <stdio.h>
#include <stdlib.h> // realloc()
#include <string.h> // strtok(), strxxxx()
#include <ctype.h> // tolower(), isxdigit()

#include "commandProcessor.h"
#include "debug.h"
#include "imageLoader.h"
#include "frameBuffer.h"
#include "matrixDriver.h"
#include "clockDisplay.h"


// forward declarations
char *lsh_read_line(void);
const char **lsh_split_line(char *line, int *tokenCtOut);
int perform(int argc, const char *argv[]);
//   (misc. utilities)
int stricmp(char const *a, char const *b);
int stringHasSuffix(const char *str, const char *suffix);
int stringHasPrefix(const char *str, const char *prefix);
int getValueOfColorSpec(const char *colorSpec);
int stringIsHexValue(const char *colorSpec);
int isHexDigitsString(const char *posHexDigits);


// ----------------------------------------------------------------------------
//  Main Entry point
//
static int s_nCurrentBufferIdx = 0;

void processCommands(int argc, const char *argv[])
{
    char *line;
    const char **args;
    int argCt;
    int status;

    printf("- processCommands() argc=(%d)\n", argc);
    for(int argCt=0; argCt<argc; argCt++) {
        printf("- arg[%d] = [%s]\n", argCt, argv[argCt]);
    }

    // if argc > 0 then do commands and return
    // else  go "interactive"!
    if(argc > 0) {
        // perform single command
        perform(argc, argv);
    }
    else {
        do {
            printf("\nmatrix> ");
            line = lsh_read_line();
            args = lsh_split_line(line, &argCt);
            status = perform(argCt, args);

            free(line);
            free(args);
        } while (status);
    }
}

// forward declarations for command functions
int commandHelp(int argc, const char *argv[]);
int commandQuit(int argc, const char *argv[]);
int commandLoadBmpFile(int argc, const char *argv[]);
int commandAllocBuffers(int argc, const char *argv[]);
int commandSelectBuffer(int argc, const char *argv[]);
int commandFillBuffer(int argc, const char *argv[]);
int commandWriteBuffer(int argc, const char *argv[]);
int commandClearBuffer(int argc, const char *argv[]);
int commandShowClock(int argc, const char *argv[]);

struct _commandEntry {
    char *name;
    char *description;
    int   minParamCount;
    int   maxParamCount;
    int (*pCommandFunction)(int argc, const char *argv[]);
} commands[] = {
    { "buffers",     "buffers {numberOfBuffers} - allocate N buffers", 1, 1, &commandAllocBuffers },
    { "buffer",      "buffer {bufferNumber} - select buffer for next actions", 1, 1, &commandSelectBuffer },
    { "clear",       "clear {selectedBuffers} - where selected is [N, N-M, ., all]", 1, 1, &commandClearBuffer },
    { "freebuffers", "freebuffers - release all buffers", 0, 0 },
    { "fill",        "fill {selectedBuffers} {fillColor} - where selected is [N, N-M, ., all] and color is [red, 0xffffff, all]", 2, 2, &commandFillBuffer },
    { "border",      "border {width} {borderColor} {indent}", 2, 3 },
    { "clock",       "clock {clockType} [{faceColor}] - where type is [digital, binary, stop] and color is [red, 0xffffff]", 1, 2, &commandShowClock },
    { "write",       "write {selectedBuffers} [{loopYN} {rate}] - where selected is [N, N-M, ., all]", 1, 3, &commandWriteBuffer },
    { "square",      "square {boarderWidth} {height} {borderColor} {fillColor}", 3, 4 },
    { "circle",      "circle {boarderWidth} {radius}  {borderColor} {fillColor}", 3, 4 },
    { "triangle",    "triangle  {boarderWidth} {baseWidth-odd!}  {borderColor} {fillColor}", 3, 4 },
    { "copy",        "copy {srcBufferNumber} {destBufferNumber} {shiftUpDownPix} {shiftLeftRightPix}", 4, 4 },
    { "default",     "default [fill|line] {color} - set default colors for subsequent draw commands", 2, 2 },
    { "moveto",      "moveto x y - move (pen) to X, Y", 2, 2 },
    { "lineto",      "lineto x y - draw line from curr X,Y to new X,Y", 2, 2 },
    { "loadbmpfile", "loadbmpfile {bmpFileName} - load 24-bit bitmap into current buffer", 1, 1, &commandLoadBmpFile },
    { "loadscreensfile", "loadscreensfile {screenSetFileName} - sets NbrScreensLoaded, ensures sufficient buffers allocated, starting from current buffer", 1, 1 },
    { "loadcmdfile", "loadcmdfile {commandsFileName} - iterates over commands read from file, once.", 1, 1 },
    { "helpcommands", "helpcommands - display list of available commands", 0, 0, &commandHelp },
    { "quit",         "quit - exit command processor", 0, 0, &commandQuit },
    { "exit",         "exit - exit command processor", 0, 0, &commandQuit },
};
static int commandCount = sizeof(commands) / sizeof(struct _commandEntry);


#define CMD_NOT_FOUND (-1)
#define CMD_RET_SUCCESS 1
#define CMD_RET_UNKNOWN_CMD 2
#define CMD_RET_BAD_PARMS 3
#define CMD_RET_EXIT 0

static int s_nCurrentCmdIdx = CMD_NOT_FOUND;


int perform(int argc, const char *argv[])
{
    int cmdIdx;
    int execStatus = CMD_RET_UNKNOWN_CMD;  // unknown command

    s_nCurrentCmdIdx = CMD_NOT_FOUND;

    // process a single command with options
    debugMessage("- perform() argc=(%d)", argc);
    for(int argCt=0; argCt<argc; argCt++) {
        debugMessage("- arg[%d] = [%s]", argCt, argv[argCt]);
    }

    for(cmdIdx = 0; cmdIdx < commandCount; cmdIdx++) {
        if(stricmp(argv[0], commands[cmdIdx].name) == 0) {
            s_nCurrentCmdIdx = cmdIdx;
            break;
        }
    }
    if(s_nCurrentCmdIdx != CMD_NOT_FOUND && commands[cmdIdx].pCommandFunction != NULL) {
        // execute the related command function, if we have correct number of params
        if(argc >= commands[cmdIdx].minParamCount + 1 && argc <= commands[cmdIdx].maxParamCount + 1) {
            execStatus = (*commands[cmdIdx].pCommandFunction)(argc, argv);
        }
        else {
            infoMessage("  --> Invalid Parameter Count for [%s] %d vs %d-%d", argv[0], argc - 1, commands[cmdIdx].minParamCount, commands[cmdIdx].maxParamCount);
            infoMessage("  USAGE: %s\n", commands[cmdIdx].description);
            execStatus = CMD_RET_BAD_PARMS;
        }
    }
    else if(s_nCurrentCmdIdx != CMD_NOT_FOUND && commands[cmdIdx].pCommandFunction == NULL) {
        // show that this command is not yet implemented
        infoMessage("** Command [%s] NOT YET IMPLEMENTED", argv[0]);
    }
    else {
        // show that we don't know this command
        warningMessage("** Unknown Command [%s]", argv[0]);
        warningMessage("   (enter 'helpcommands' to show full list of commands)\n");
    }
    return execStatus;
}

// ----------------------------------------------------------------------------
//  COMMAND Functions
//
int commandShowClock(int argc, const char *argv[])
{
    int bValidCommand = 1;

    // IMPLEMENT:
    //   clear {selectedBuffers} - where selected is [N, N-M, ., all]
    if(stricmp(argv[0], commands[s_nCurrentCmdIdx].name) != 0) {
        errorMessage("[CODE]: bad call commandClearBuffer with command [%s]", argv[0]);
        bValidCommand = 0;
    }
    else if(argc - 1 < 1) {
        errorMessage("[CODE]: bad call - param count err for command [%s]", argv[0]);
        bValidCommand = 0;
    }
    if(bValidCommand) {
        // which type of clock?
        eClockFaceTypes clockType = CFT_Unknown;
        if(stricmp(argv[1], "binary") == 0) {
            clockType = CFT_BINARY;
        }
        else if(stricmp(argv[1], "digital") == 0) {
             clockType = CFT_DIGITAL;
        }
        else if(stricmp(argv[1], "stop") == 0) {
            clockType = CFT_NO_CLOCK;  // stop it if running
        }
        if(clockType == CFT_Unknown) {
            errorMessage("Must specify type of clock face [digital|binary]");
        }
        else if(clockType == CFT_NO_CLOCK) {
            // user just wants to stop the clock
            if(isClockRunning()) {
                stopClock();
            }
        }
        else {
            int nFaceColor = 0x808080;	// grey unless spec'd
    	    if((argc - 1) == 2) {
                nFaceColor = getValueOfColorSpec(argv[2]);
    	    }
            debugMessage("nFaceColor=(0x%.6X) clockType=[%s]",nFaceColor, argv[1]);
            // stop clock if already running
            if(isClockRunning()) {
                stopClock();
            }
            // run latest version selected
            runClock(clockType, nFaceColor, s_nCurrentBufferIdx+1);
        }
    }
    return CMD_RET_SUCCESS;   // no errors
}

int commandClearBuffer(int argc, const char *argv[])
{
    int bValidCommand = 1;

    // IMPLEMENT:
    //   clear {selectedBuffers} - where selected is [N, N-M, ., all]
    if(stricmp(argv[0], commands[s_nCurrentCmdIdx].name) != 0) {
        errorMessage("[CODE]: bad call commandClearBuffer with command [%s]", argv[0]);
        bValidCommand = 0;
    }
    else if(argc - 1 != 1) {
        errorMessage("[CODE]: bad call - param count err for command [%s]", argv[0]);
        bValidCommand = 0;
    }
    if(bValidCommand) {
        // FIXME: UNDONE argv[1] is really a buffer spec, interpret it into fromBuffer, toBuffer!
        int nBufferNumber = atoi(argv[1]);
        int nMaxBuffers = numberBuffers();
        debugMessage("nBufferNumber=(%d)",nBufferNumber);
        if(nBufferNumber < 1 || nBufferNumber > nMaxBuffers) {
           errorMessage("Buffer (%d) out-of-range: [must be 1 >= N <= %d]", nMaxBuffers);
        }
        else {
            int nFillColor = 0; // always clear to black
            debugMessage("nFillColor=(0x%.6X)",nFillColor);
            // now set fill color to selected buffer
            fillBufferWithColorRGB(nBufferNumber, nFillColor);
        }
    }
    return CMD_RET_SUCCESS;   // no errors
}

int commandWriteBuffer(int argc, const char *argv[])
{
    int bValidCommand = 1;

    // IMPLEMENT:
    //   write {selectedBuffers} {loopYN} {rate} - where selected is [N, N-M, ., all]
    if(stricmp(argv[0], commands[s_nCurrentCmdIdx].name) != 0) {
        errorMessage("[CODE]: bad call commandWriteBuffer with command [%s]", argv[0]);
        bValidCommand = 0;
    }
    else if(argc < 2 || argc > 4) {
        errorMessage("[CODE]: bad call - param count err for command [%s]", argv[0]);
        bValidCommand = 0;
    }
    if(bValidCommand) {
        // FIXME: UNDONE argv[1] is really a buffer spec, interpret it into fromBuffer, toBuffer!
        // FIXME: UNDONE PARMS argv[2] and argv[3] are NOT YET IMPLEMENTED
        int nBufferNumber = atoi(argv[1]);
        int nMaxBuffers = numberBuffers();
        debugMessage("nBufferNumber=(%d)",nBufferNumber);
        if(nBufferNumber < 1 || nBufferNumber > nMaxBuffers) {
           errorMessage("Buffer (%d) out-of-range: [must be 1 >= N <= %d]", nMaxBuffers);
        }
        else {
            // now write buffer N contents to matrix itself
            int nBufferSize = frameBufferSizeInBytes();
            uint8_t *pCurrBuffer = (uint8_t *)ptrBuffer(nBufferNumber);
            showBuffer(pCurrBuffer, nBufferSize);
        }
    }
    return CMD_RET_SUCCESS;   // no errors
}

int commandFillBuffer(int argc, const char *argv[])
{
    int bValidCommand = 1;

    // IMPLEMENT:
    //   fill {selectedBuffers} {fillColor} - where selected is [N, N-M, ., all] and color is [red, 0xffffff, all]
    if(stricmp(argv[0], commands[s_nCurrentCmdIdx].name) != 0) {
        errorMessage("[CODE]: bad call commandFillBuffer with command [%s]", argv[0]);
        bValidCommand = 0;
    }
    else if(argc - 1 != 2) {
        errorMessage("[CODE]: bad call - param count err for command [%s]", argv[0]);
        bValidCommand = 0;
    }
    if(bValidCommand) {
        // FIXME: UNDONE argv[1] is really a buffer spec, interpret it into fromBuffer, toBuffer!
        int nBufferNumber = atoi(argv[1]);
        int nMaxBuffers = numberBuffers();
        debugMessage("nBufferNumber=(%d)",nBufferNumber);
        if(nBufferNumber < 1 || nBufferNumber > nMaxBuffers) {
           errorMessage("Buffer (%d) out-of-range: [must be 1 >= N <= %d]", nMaxBuffers);
        }
        else {
            int nFillColor = getValueOfColorSpec(argv[2]);
            debugMessage("nFillColor=(0x%.6X)",nFillColor);
            // now set fill color to selected buffer
            fillBufferWithColorRGB(nBufferNumber, nFillColor);
        }
    }
    return CMD_RET_SUCCESS;   // no errors
}


int commandSelectBuffer(int argc, const char *argv[])
{
    int bValidCommand = 1;

    // IMPLEMENT:
    //   buffer {bufferNumber} - select buffer for next actions
    if(stricmp(argv[0], commands[s_nCurrentCmdIdx].name) != 0) {
        errorMessage("[CODE]: bad call commandAllocBuffers with command [%s]", argv[0]);
        bValidCommand = 0;
    }
    else if(argc - 1 != 1) {
        errorMessage("[CODE]: bad call - param count err for command [%s]", argv[0]);
        bValidCommand = 0;
    }
    if(bValidCommand) {
        int nBufferNumber = atoi(argv[1]);
        int nMaxBuffers = numberBuffers();
        //debugMessage("alloc %d buffers...",nBuffers);
        if(nBufferNumber < 1 || nBufferNumber > nMaxBuffers) {
           errorMessage("Buffer (%d) out-of-range: [must be 1 >= N <= %d]", nMaxBuffers);
        }
        else {
            // set new current buffer for use by later commands
            s_nCurrentBufferIdx = nBufferNumber - 1;
            debugMessage("Selected buffer #%d", nBufferNumber);
        }
    }
    return CMD_RET_SUCCESS;   // no errors
}

int commandAllocBuffers(int argc, const char *argv[])
{
    int bValidCommand = 1;

    // IMPLEMENT:
    //   buffers {numberOfBuffers} - allocate N buffers
    if(stricmp(argv[0], commands[s_nCurrentCmdIdx].name) != 0) {
        errorMessage("[CODE]: bad call commandAllocBuffers with command [%s]", argv[0]);
        bValidCommand = 0;
    }
    else if(argc - 1 != 1) {
        errorMessage("[CODE]: bad call - param count err for command [%s]", argv[0]);
        bValidCommand = 0;
    }
    if(bValidCommand) {
        int nBuffers = atoi(argv[1]);
        //debugMessage("alloc %d buffers...",nBuffers);
        if(nBuffers > 0) {
            // ensure requested number of buffers are allocated
            allocBuffers(nBuffers);
        }
        else {
            errorMessage("[CODE]: bad call - param value [converts as 0: %s]", argv[1]);
        }
    }
    return CMD_RET_SUCCESS;   // no errors
}

int commandLoadBmpFile(int argc, const char *argv[])
{
    int bValidCommand = 1;

    // IMPLEMENT:
    //   loadbmpfile {bmpFileName} - load 24-bit bitmap into current buffer
    if(stricmp(argv[0], commands[s_nCurrentCmdIdx].name) != 0) {
        errorMessage("[CODE]: bad call commandLoadBmpFile with command [%s]", argv[0]);
        bValidCommand = 0;
    }
    else if(argc - 1 != 1) {
        errorMessage("[CODE]: bad call - param count err for command [%s]", argv[0]);
        bValidCommand = 0;
    }
    if(bValidCommand) {
        const char *fileSpec = argv[1];
        if(!stringHasSuffix(fileSpec, ".bmp")) {
            warningMessage("Invalid filetype [%s], expected [.bmp]", fileSpec);
            bValidCommand = 0;
        }
        if(bValidCommand) {
            if(fileExists(fileSpec)) {
                int nImageSize;
                loadImageFromFile(fileSpec, &nImageSize);
                int nBufferSize = frameBufferSizeInBytes();
                if(nImageSize != nBufferSize) {
                    warningMessage("Filesize (%d bytes) incorrect for 32x24 matrix (%d bytes), display aborted!", nImageSize, nBufferSize);
                }
                else {
                    uint8_t *pCurrBuffer = (uint8_t *)ptrBuffer(s_nCurrentBufferIdx + 1);
                    xlateLoadedImageIntoBuffer(pCurrBuffer, nBufferSize);
                }
            }
        }
    }

    return CMD_RET_SUCCESS;   // no errors
}

int commandHelp(int argc, const char *argv[])
{
    int cmdIdx;
    printf("\n--- Available Commands ---\n");
    for(cmdIdx = 0; cmdIdx < commandCount; cmdIdx++) {
        printf("  %s\n", commands[cmdIdx].description);
    }
    printf("--- ------------------ ---\n\n");
    return CMD_RET_SUCCESS;   // no errors
}

int commandQuit(int argc, const char *argv[])
{
    // returning zero causes command interpreter to exit
    return CMD_RET_EXIT;
}


// ----------------------------------------------------------------------------
//  PRIVATE Functions
//
//
// REF borrowing from: https://brennan.io/2015/01/16/write-a-shell-in-c
//
char *lsh_read_line(void)
{
    char *line = NULL;
    ssize_t bufsize = 0; // have getline allocate a buffer for us
    getline(&line, &bufsize, stdin);
    return line;
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

const char **lsh_split_line(char *line, int *tokenCtOut)
{
    int bufsize = LSH_TOK_BUFSIZE;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;

    // if requested, pass count of tokens found to caller
    if(tokenCtOut != NULL) {
        *tokenCtOut = position;
    }
    return (const char **)tokens;
}

int getValueOfColorSpec(const char *colorSpec)
{
    int desiredValue = 0;
    if(stringIsHexValue(colorSpec)) {
        desiredValue = (int)strtol(colorSpec, NULL, 16);
    }
    else if(stricmp(colorSpec, "red") == 0) {
       desiredValue = 0xff0000;
    }
    else if(stricmp(colorSpec, "green") == 0) {
       desiredValue = 0x00ff00;
    }
    else if(stricmp(colorSpec, "blue") == 0) {
       desiredValue = 0x0000ff;
    }
    else if(stricmp(colorSpec, "cyan") == 0) {
       desiredValue = 0x00ffff;
    }
    else if(stricmp(colorSpec, "yellow") == 0) {
       desiredValue = 0xffff00;
    }
    else if(stricmp(colorSpec, "magenta") == 0) {
       desiredValue = 0xff00ff;
    }
    else if(stricmp(colorSpec, "white") == 0) {
       desiredValue = 0xffffff;
    }
    else if(stricmp(colorSpec, "black") == 0) {
       desiredValue = 0x000000;
    }
    else if(stricmp(colorSpec, "silver") == 0) {
       desiredValue = 0xc0c0c0;
    }
    else if(stricmp(colorSpec, "gray") == 0) {
       desiredValue = 0x808080;
    }
    else {
        warningMessage("Failed to decode colorSpec[%s]", colorSpec);
    }
    //debugMessage("desiredValue=0x%.6X",desiredValue);
    return desiredValue;
}

int stringIsHexValue(const char *colorSpec)
{
    int hexStatus = 0;  // NO!
    if(stringHasPrefix(colorSpec, "0x") && isHexDigitsString(&colorSpec[2])) {
        hexStatus = 1;  // YES!
    }
    else if(isHexDigitsString(colorSpec)) {
        hexStatus = 1;  // YES!
    }
    //debugMessage("stringIsHexValue()=%d",hexStatus);
    return hexStatus;
}

int isHexDigitsString(const char *posHexDigits)
{
    int nLength = strlen(posHexDigits);
    int hexStatus = 1; // YES is all hex digits
    for(int nCharIdx = 0; nCharIdx < nLength; nCharIdx++) {
        if(!isxdigit(posHexDigits[nCharIdx])) {
            hexStatus = 0; // No
            break;  // done, we have our answer, abort loop
        }
    }
    //debugMessage("isHexDigitsString()=%d",hexStatus);
    return hexStatus;
}

// ----------------------------------------------------------------------------
//  Missing Functions
//
// REF: https://stackoverflow.com/questions/5820810/case-insensitive-string-comp-in-c
int stricmp(char const *a, char const *b)
{
    const char *aOrig = a;
    const char *bOrig = b;
    if(a == NULL && b != NULL) {
    return -99;
    }
    else if(a != NULL && b == NULL) {
    return 99;
    }
    else if(a == NULL && b == NULL) {
    return -101;
    }
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a) {
        //debugMessage("stricmp(%s,%s)==%d", aOrig, bOrig, d);
            return d;
        }
    }
}

int stringHasSuffix(const char *str, const char *suffix)
{
    if (!str || !suffix) {
        return 0;
    }
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr) {
        return 0;
    }
    return stricmp(str + lenstr - lensuffix, suffix) == 0;
}

int stringHasPrefix(const char *str, const char *prefix)
{
    if (!str || !prefix) {
        return 0;
    }
    size_t lenstr = strlen(str);
    size_t lenprefix = strlen(prefix);
    if (lenprefix > lenstr) {
        return 0;
    }
    return strncmp(str + lenstr - lenprefix, prefix, lenprefix) == 0;
}

