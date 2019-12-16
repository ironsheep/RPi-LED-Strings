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
#include "xmalloc.h"
#include "imageLoader.h"
#include "frameBuffer.h"
#include "matrixDriver.h"
#include "clockDisplay.h"


// forward declarations
char *lsh_read_line(void);
const char **lsh_split_line(char *line, int *tokenCtOut);
int perform(int argc, const char *argv[]);

//   (misc. utilities)
struct _bufferSpec {
    uint16_t fmBufferNumber;
    uint16_t toBufferNumber;
    uint16_t nMaxBuffers;
};

int stricmp(char const *a, char const *b);
int stringHasSuffix(const char *str, const char *suffix);
int stringHasPrefix(const char *str, const char *prefix);
struct _bufferSpec *getBufferNumbersFromBufferSpec(const char *bufferSpec);
int getValueOfColorSpec(const char *colorSpec);
int getPanelNumberFromPanelSpec(const char *panelSpec);
int stringIsHexValue(const char *colorSpec);
int isHexDigitsString(const char *posHexDigits);
void combineTokens(char **tokens, int ltIdx, int rtIdx);
char *strconcat(char *str1, char *str2);


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
int commandSetBorder(int argc, const char *argv[]);
int commandColorToScreen(int argc, const char *argv[]);
int commandStringToScreen(int argc, const char *argv[]);

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
    // huh!  p[1-3|12,23], screen as new buffer types? which use current buffer and write directly to screen!
    { "screen",      "screen {fillcolor|clear} [{panelSpec}]  - clear(or fill) single panel or entire screen", 1, 2, &commandColorToScreen },
    { "string",      "string {selectedBuffers} {string} {lineColor} [{panelSpec}] - write string to screen w/wrap (or just single panel)", 3, 4, &commandStringToScreen },
    { "fill",        "fill {selectedBuffers} {fillColor} - where selected is [N, N-M, ., all] and color is [red, 0xffffff, all]", 2, 2, &commandFillBuffer },
    { "border",      "border {width} {borderColor} {panelSpec} [{indent}] - draw border of color", 3, 4, &commandSetBorder },
    { "clock",       "clock {clockType} [{faceColor} {panelNumber-digiOnly}]  - where type is [digital, binary, stop] and color is [red, 0xffffff]", 1, 3, &commandShowClock },
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
    if(argc > 0) {
        debugMessage("- perform() argc=(%d)", argc);
        for(int argCt=0; argCt<argc; argCt++) {
            debugMessage("- arg[%d] = [%s]", argCt, argv[argCt]);
        }
    }
    else {
        return CMD_RET_SUCCESS;
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
    else if(argc == 0) {
	    // do nothing, just let prompt show again...
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
int commandStringToScreen(int argc, const char *argv[])
{
    int bValidCommand = 1;

    // IMPLEMENT:
    //   string {selectedBuffers} {string} {color} [{[p[1-3]}] - write string to screen w/wrap (or just single panel)
    if(stricmp(argv[0], commands[s_nCurrentCmdIdx].name) != 0) {
        errorMessage("[CODE]: bad call commandColorToScreen with command [%s]", argv[0]);
        bValidCommand = 0;
    }
    else if(argc - 1 < 3 || argc - 1 > 4) {
        errorMessage("[CODE]: bad call - param count err for command [%s]", argv[0]);
        bValidCommand = 0;
    }
    if(bValidCommand) {
        struct _bufferSpec *bufferSpec = getBufferNumbersFromBufferSpec(argv[1]);
        if(bufferSpec->fmBufferNumber < 1) {
            errorMessage("Buffer (%d) out-of-range: [must be 1 >= N <= %d]", bufferSpec->fmBufferNumber, bufferSpec->nMaxBuffers);
            bValidCommand = 0;
        }
        const char *cString = argv[2];
        if(bValidCommand) {
            debugMessage("cString=[%s]",cString);
            if(strlen(cString) < 1) {
                errorMessage("[CODE]: bad call - can't write an empty string [%s]", argv[0]);
                bValidCommand = 0;
            }
        }
        int nFillColor = getValueOfColorSpec(argv[3]);
        debugMessage("nFillColor=(0x%.6X)",nFillColor);
        if(bValidCommand) {
            int nPanelNumber = 0;   // 0 == all panels
            if((argc - 1) == 4) {
                nPanelNumber = getPanelNumberFromPanelSpec(argv[4]);
            }
            debugMessage("nPanelNumber=(%d)",nPanelNumber);
            if(nPanelNumber < 0) {
               errorMessage("Panel (%d) out-of-range: [must be 1 >= N <= %d, 12, or 23]", NUMBER_OF_PANELS);
            }
            else {
                // FIXME: UNDONE - Handle range of buffers case
                if(nPanelNumber == 0) {
                    // write string to full screen wrapping and end of each panel
                    writeStringToBufferWithColorRGB(bufferSpec->fmBufferNumber, cString, nFillColor);
                }
                else {
                     // write string to specified panel truncating at end of panel
                    writeStringToBufferPanelWithColorRGB(bufferSpec->fmBufferNumber, cString, nPanelNumber, nFillColor);
                }
                // lastly immediately write to screen
                int nBufferSize = frameBufferSizeInBytes();
                uint8_t *pCurrBuffer = (uint8_t *)ptrBuffer(bufferSpec->fmBufferNumber);
                showBuffer(pCurrBuffer, nBufferSize);
                // FIXME: UNDONE - if multiple buffers send all at once (since is range of buffers)
            }
        }
        free(bufferSpec);
    }
    return CMD_RET_SUCCESS;   // no errors
}

int commandColorToScreen(int argc, const char *argv[])
{
    int bValidCommand = 1;

    // IMPLEMENT:
    //   screen {fillcolor|clear} [{[p[1-3]}]  - clear(or fill) single panel or entire screen
    if(stricmp(argv[0], commands[s_nCurrentCmdIdx].name) != 0) {
        errorMessage("[CODE]: bad call commandColorToScreen with command [%s]", argv[0]);
        bValidCommand = 0;
    }
    else if(argc - 1 != 2) {
        errorMessage("[CODE]: bad call - param count err for command [%s]", argv[0]);
        bValidCommand = 0;
    }
    if(bValidCommand) {
        int nFillColor = 0;
        if(stricmp(argv[1], "clear") != 0) {
            nFillColor = getValueOfColorSpec(argv[1]);
        }
        debugMessage("nFillColor=(0x%.6X)",nFillColor);
        int nPanelNumber = 0;   // 0 = all panels
        if((argc -1) == 2) {
            nPanelNumber = getPanelNumberFromPanelSpec(argv[2]);
        }
        debugMessage("nPanelNumber=(%d)",nPanelNumber);
        if(nPanelNumber < 0) {
           errorMessage("Panel (%d) out-of-range: [must be 1 >= N <= %d, 12, or 23]", NUMBER_OF_PANELS);
        }
        else {
            if(nPanelNumber == 0) {
                // now set fill color to selected buffer
                fillBufferWithColorRGB(s_nCurrentBufferIdx+1, nFillColor);
            }
            else {
                 // now set fill color to selected panel in buffer
                fillBufferPanelWithColorRGB(s_nCurrentBufferIdx+1, nPanelNumber, nFillColor);
            }
            // lastly immediately write to screen
            int nBufferSize = frameBufferSizeInBytes();
            uint8_t *pCurrBuffer = (uint8_t *)ptrBuffer(s_nCurrentBufferIdx+1);
            showBuffer(pCurrBuffer, nBufferSize);
        }
    }
    return CMD_RET_SUCCESS;   // no errors
}


int commandSetBorder(int argc, const char *argv[])
{
       int bValidCommand = 1;

    // IMPLEMENT:
    //   border {width} {borderColor} {panelSpec}  [{indent}] - draw border of color
    if(stricmp(argv[0], commands[s_nCurrentCmdIdx].name) != 0) {
        errorMessage("[CODE]: bad call commandSetBorder with command [%s]", argv[0]);
        bValidCommand = 0;
    }
    else if(argc - 1 < 1) {
        errorMessage("[CODE]: bad call - param count err for command [%s]", argv[0]);
        bValidCommand = 0;
    }
    if(bValidCommand) {
        // which specs for border?
        int nLineWidthInPix = atoi(argv[1]);
        uint32_t nLineColor = getValueOfColorSpec(argv[2]);
        int nPanelNumber = 0;   // 0 = all panels
        if((argc - 1) > 2) {
            nPanelNumber = getPanelNumberFromPanelSpec(argv[3]);
        }
        debugMessage("nPanelNumber=(%d)",nPanelNumber);
        if(nPanelNumber < 0) {
           errorMessage("Panel (%d) out-of-range: [must be 1 >= N <= %d, 12, or 23]", NUMBER_OF_PANELS);
        }
        uint8_t nIndentInPix = 0;
        if((argc - 1) == 4) {
            nLineWidthInPix = atoi(argv[4]);
        }
        if(nLineWidthInPix >= 0 && nLineWidthInPix <= 12) {
            int nOffsetX = nIndentInPix;
            int nOffsetY = nIndentInPix;
            int nBorderWidth = 32 - (2 * nIndentInPix);
            int nBorderHeight = 24 - (2 * nIndentInPix);
            drawSquareInBuffer(s_nCurrentBufferIdx+1, nOffsetX, nOffsetY, nPanelNumber, nBorderWidth, nBorderHeight, nLineWidthInPix, nLineColor);
        }
        else {
            errorMessage("[CODE]: bad param(s) - line width too small! (%d not in range [1-12])", nLineWidthInPix);
        }
    }
    return CMD_RET_SUCCESS;   // no errors
}

int commandShowClock(int argc, const char *argv[])
{
    int bValidCommand = 1;

    // IMPLEMENT:
    //   clock {clockType} [{faceColor} {panelNumber-digiOnly}]  - where type is [digital, binary, stop] and color is [red, 0xffffff]
    if(stricmp(argv[0], commands[s_nCurrentCmdIdx].name) != 0) {
        errorMessage("[CODE]: bad call commandClearBuffer with command [%s]", argv[0]);
        bValidCommand = 0;
    }
    else if((argc - 1) < 1 || (argc - 1) > 3) {
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
    	    if((argc - 1) > 1) {
                nFaceColor = getValueOfColorSpec(argv[2]);
    	    }
            debugMessage("nFaceColor=(0x%.6X) clockType=[%s]",nFaceColor, argv[1]);
            int nPanelNumber = 0;   // 0 = all panels
            if((argc - 1) > 2) {
                nPanelNumber = getPanelNumberFromPanelSpec(argv[3]);
            }
            debugMessage("nPanelNumber=(%d)",nPanelNumber);
            // stop clock if already running
            if(isClockRunning()) {
                stopClock();
            }
            // run latest version selected
            runClock(clockType, nFaceColor, s_nCurrentBufferIdx+1, nPanelNumber);
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
        struct _bufferSpec *bufferSpec = getBufferNumbersFromBufferSpec(argv[1]);
        if(bufferSpec->fmBufferNumber < 1) {
           errorMessage("Buffer (%d) out-of-range: [must be 1 >= N <= %d]", bufferSpec->fmBufferNumber, bufferSpec->nMaxBuffers);
        }
        else {
            int nFillColor = 0; // always clear to black
            debugMessage("nFillColor=(0x%.6X)",nFillColor);
            // now set fill color to selected buffer
            // FIXME: UNDONE - Handle range of buffers case
            fillBufferWithColorRGB(bufferSpec->fmBufferNumber, nFillColor);
        }
        free(bufferSpec);
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
        // FIXME: UNDONE PARMS argv[2] and argv[3] are NOT YET IMPLEMENTED
        struct _bufferSpec *bufferSpec = getBufferNumbersFromBufferSpec(argv[1]);
        if(bufferSpec->fmBufferNumber < 1) {
           errorMessage("Buffer (%d) out-of-range: [must be 1 >= N <= %d]", bufferSpec->fmBufferNumber, bufferSpec->nMaxBuffers);
        }
        else {
            // now write buffer N contents to matrix itself
            // FIXME: UNDONE - Handle range of buffers case
            int nBufferSize = frameBufferSizeInBytes();
            uint8_t *pCurrBuffer = (uint8_t *)ptrBuffer(bufferSpec->fmBufferNumber);
            showBuffer(pCurrBuffer, nBufferSize);
        }
        free(bufferSpec);
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
        struct _bufferSpec *bufferSpec = getBufferNumbersFromBufferSpec(argv[1]);
        if(bufferSpec->fmBufferNumber < 1) {
           errorMessage("Buffer (%d) out-of-range: [must be 1 >= N <= %d]", bufferSpec->fmBufferNumber, bufferSpec->nMaxBuffers);
        }
        else {
            int nFillColor = getValueOfColorSpec(argv[2]);
            debugMessage("nFillColor=(0x%.6X)",nFillColor);
            // now set fill color to selected buffer
            // FIXME: UNDONE - Handle range of buffers case
            fillBufferWithColorRGB(bufferSpec->fmBufferNumber, nFillColor);
        }
        free(bufferSpec);
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

    // now let's put back together double-quoted strings
    int ltIdx = 0;
    do {
	int ltTokenLen = strlen(tokens[ltIdx]);
	if(tokens[ltIdx][0] == '"' && tokens[ltIdx][ltTokenLen - 1] != '"') {
	    int rtIdx = ltIdx + 1;
	    int groupedTokensCt = 1;
	    do {
		    int rtTokenLen = strlen(tokens[rtIdx]);
	        if(tokens[rtIdx][rtTokenLen - 1] == '"') {
                combineTokens(tokens, ltIdx, rtIdx);
		        ltIdx = -1; // so that next incr makes ZERO!;
		        break; // restart outer loop
		    }
		    else {
		        groupedTokensCt += 1;
		        rtIdx += 1;
		    }
        } while(tokens[rtIdx] != NULL);
	}
	ltIdx++;
    } while (tokens[ltIdx] != NULL);

    // now fixup position
    position = 0;
    do {
        if(tokens[position] != NULL) {
	        position += 1;
	    }
    } while(tokens[position] != NULL);

    // if requested, pass count of tokens found to caller
    if(tokenCtOut != NULL) {
        *tokenCtOut = position;
    }
    return (const char **)tokens;
}

void combineTokens(char **tokens, int ltIdx, int rtIdx)
{
    // append tokens[ltIdx] thru tokens[rtIdx]
    int targetIdx = ltIdx;
    int currIdx = ltIdx + 1;
    do {
        tokens[targetIdx] = strconcat(tokens[targetIdx], tokens[currIdx]);
        tokens[currIdx++] = NULL;
    } while(currIdx <= rtIdx);

    // then move all forward shortening the entire list
    int destIdx = ltIdx + 1;
    int srcIdx = rtIdx + 1;
    do {
        if(tokens[srcIdx] != NULL) {
            tokens[destIdx++] = tokens[srcIdx];
            tokens[srcIdx++] = NULL;
        }
    } while(tokens[srcIdx] != NULL);

    // lastly remove double-quotes from combined token
    if(tokens[targetIdx][0] == '"') {
       int targetLen = strlen(tokens[targetIdx]);
       tokens[targetIdx][targetLen - 1] = 0x00;
       tokens[targetIdx] = &tokens[targetIdx][1];
    }
}

char *strconcat(char *str1, char *str2)
{
    char *result = xmalloc(strlen(str1) + 1 + strlen(str2) + 1); // +1 for the null-terminator and for the space between
    strcpy(result, str1);
    strcat(result, " ");
    strcat(result, str2);
    return result;
}

struct _bufferSpec *getBufferNumbersFromBufferSpec(const char *bufferSpec)
{
    //debugMessage("getBufferNumbersFromBufferSpec(%s) - ENTRY", bufferSpec);
    //parse [N, N-M, ., all]
    int nMaxBuffers = numberBuffers();
    struct _bufferSpec *returnSpec = xmalloc(sizeof(struct _bufferSpec));
    returnSpec->nMaxBuffers = nMaxBuffers;

    if(stricmp(bufferSpec, ".")) {
        // return list of just the current buffer
        returnSpec->fmBufferNumber = s_nCurrentBufferIdx + 1;
        returnSpec->toBufferNumber = returnSpec->fmBufferNumber;
    }
    else if(stricmp(bufferSpec, "all")) {
        // return list of all buffers
        returnSpec->fmBufferNumber = 1;
        returnSpec->toBufferNumber = nMaxBuffers;
    }
    else if(strchr(bufferSpec, '-') != NULL) {
        const char *rtStr = strchr(bufferSpec, '-');
        // return list of buffers: from-to
        returnSpec->fmBufferNumber = atoi(bufferSpec);
        returnSpec->toBufferNumber = atoi(&rtStr[1]); // skip the '-' char
        if(returnSpec->toBufferNumber < returnSpec->fmBufferNumber) {
            returnSpec->toBufferNumber = returnSpec->fmBufferNumber;
            errorMessage("bad buffer spec [%s] ignored 'to' spec!", bufferSpec);
        }
    }
    else {
        // return just the specfic buffer
        returnSpec->fmBufferNumber = atoi(bufferSpec);
        returnSpec->toBufferNumber = returnSpec->fmBufferNumber;
    }

    // NOTE: code not implemented yet...
    if(returnSpec->fmBufferNumber != returnSpec->toBufferNumber) {
       errorMessage("[CODE] Buffer(from,to)=(%d,%d) 'Range of buffers' is NOT YET SUPPORTED", returnSpec->fmBufferNumber, returnSpec->toBufferNumber);
    }

    // validate results
    if(returnSpec->fmBufferNumber < 1 || returnSpec->fmBufferNumber > nMaxBuffers) {
        errorMessage("Buffer(from) (%d) out-of-range: [must be 1 >= N <= %d]", returnSpec->fmBufferNumber, nMaxBuffers);
        returnSpec->fmBufferNumber = 0; // mark bad spec!
        returnSpec->toBufferNumber = 0;
    }
    else if(returnSpec->toBufferNumber < 1 || returnSpec->toBufferNumber > nMaxBuffers) {
        errorMessage("Buffer(to) (%d) out-of-range: [must be 1 >= N <= %d]", returnSpec->toBufferNumber, nMaxBuffers);
        returnSpec->fmBufferNumber = 0; // mark bad spec!
        returnSpec->toBufferNumber = 0;
    }

    debugMessage("getBufferNumbersFromBufferSpec(%s) -> (%d,%d)", bufferSpec, returnSpec->fmBufferNumber, returnSpec->toBufferNumber);
    return returnSpec;
}

int getPanelNumberFromPanelSpec(const char *panelSpec)
{
    // parse string of [p1,p2,p3] returning int 1-3 -AND [p12,p23] returning 12,23
    // NEW: '*' means full screen, return 0 for this case
    int desiredValue = 0;
    if(panelSpec[0] == '*' && strlen(panelSpec) == 1) {
        desiredValue = 0; // !yes!
    }
    else if(tolower(panelSpec[0]) == 'p' && (strlen(panelSpec) == 2 || strlen(panelSpec) == 3)) {
        if(strlen(panelSpec) == 2 && panelSpec[1] >= 0x31 && panelSpec[1] <= 0x33) {
         // parse string of [p1,p2,p3] returning int 1-3
           desiredValue = panelSpec[1] - 0x30;
        }
        else if(strlen(panelSpec) == 3 && panelSpec[1] >= 0x31 && panelSpec[1] <= 0x32 && panelSpec[2] >= 0x32 && panelSpec[2] <= 0x33) {
            // parse string of [p12,p23] returning 12,23
            desiredValue = ((panelSpec[1] - 0x30) * 10) + (panelSpec[2] - 0x30);
        } else {
            desiredValue = -1;
        }
    }
    else {
        desiredValue = -1;
    }
    if(desiredValue == -1) {
        errorMessage("getPanelNumberFromPanelSpec(%s) - INVALID SPEC", panelSpec);
    }
    else {
        debugMessage("getPanelNumberFromPanelSpec(%s) = (%d)", panelSpec, desiredValue);
    }
    return desiredValue;
}


int getValueOfColorSpec(const char *colorSpec)
{
    int desiredValue = 0;
    if(stringIsHexValue(colorSpec)) {
        if(stringHasPrefix(colorSpec, "0x")) {
            desiredValue = (int)strtol(&colorSpec[2], NULL, 16);
        }
        else {
            desiredValue = (int)strtol(colorSpec, NULL, 16);
        }
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
    debugMessage("stringIsHexValue()=%d",hexStatus);
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
    debugMessage("isHexDigitsString(%s)=%d", posHexDigits, hexStatus);
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
    int checkStatus = -1;
    if (!str || !prefix) {
        checkStatus = 0;
    }
    else {
        size_t lenstr = strlen(str);
        size_t lenprefix = strlen(prefix);
        if (lenprefix > lenstr) {
            checkStatus = 0;
        }
        else {
            checkStatus = strncmp(str, prefix, lenprefix) == 0;
        }
    }
    debugMessage("stringHasPrefix([%s], [%s]) == %d", str, prefix, checkStatus);
    return checkStatus;
}

