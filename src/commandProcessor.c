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
#include <ctype.h> // tolower()

#include "commandProcessor.h"
#include "debug.h"
#include "imageLoader.h"
#include "frameBuffer.h"

char *lsh_read_line(void);
const char **lsh_split_line(char *line, int *tokenCtOut);
int perform(int argc, const char *argv[]);


int stricmp(char const *a, char const *b);
int stringHasSuffix(const char *str, const char *suffix);


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
int stricmp(char const *a, char const *b);
int commandHelp(int argc, const char *argv[]);
int commandQuit(int argc, const char *argv[]);
int commandLoadBmpFile(int argc, const char *argv[]);
int commandAllocBuffers(int argc, const char *argv[]);

struct _commandEntry {
    char *name;
    char *description;
    int   paramCount;
    int (*pCommandFunction)(int argc, const char *argv[]);
} commands[] = {
    { "buffers",     "buffers {numberOfBuffers} - allocate N buffers", 1, &commandAllocBuffers },
    { "buffer",      "buffer {bufferNumber} - select buffer for next actions", 1 },
    { "clear",       "clear {selectedBuffers} - where selected is [N, N-M, all]", 1 },
    { "freebuffers", "freebuffers - release all buffers", 0 },
    { "fill",        "fill {selectedBuffers} {fillColor} - where selected is [N, N-M, all] and color is [red, 0xffffff, all]", 2 },
    { "border",      "border {width} {borderColor}", 1 },
    { "clock",       "clock {clockType} {faceColor} - where type is [digital, binary] and color is [red, 0xffffff]", 2 },
    { "buffers",     "write {selectedBuffers} {loopYN} {rate} - where selected is [N, N-M, all]", 3 },
    { "square",      "square {boarderWidth} {height} {borderColor} {fillColor}", 4 },
    { "circle",      "circle {boarderWidth} {radius}  {borderColor} {fillColor}", 1 },
    { "triangle",    "triangle  {boarderWidth} {baseWidth-odd!}  {borderColor} {fillColor}", 1 },
    { "copy",        "copy {srcBufferNumber} {destBufferNumber} {shiftUpDownPix} {shiftLeftRightPix}", 1 },
    { "default",     "default [fill|line] {color}", 2 },
    { "moveto",      "moveto x y - move (pen) to X, Y", 2 },
    { "lineto",      "lineto x y - draw line from curr X,Y to new X,Y", 2 },
    { "loadbmpfile", "loadbmpfile {bmpFileName} - load 24-bit bitmap into current buffer", 1, &commandLoadBmpFile },
    { "loadscreensfile", "loadscreensfile {screenSetFileName} - sets NbrScreensLoaded, ensures sufficient buffers allocated, starting from current buffer", 1 },
    { "helpcommands", "helpcommands - display list of available commands", 0, &commandHelp },
    { "quit",         "quit - exit command processor", 0, &commandQuit },
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
    printf("- perform() argc=(%d)\n", argc);
    for(int argCt=0; argCt<argc; argCt++) {
    	printf("- arg[%d] = [%s]\n", argCt, argv[argCt]);
    }
    
    for(cmdIdx = 0; cmdIdx < commandCount; cmdIdx++) {
        if(stricmp(argv[0], commands[cmdIdx].name) == 0) {
            s_nCurrentCmdIdx = cmdIdx;
            break;
        }
    }
    if(s_nCurrentCmdIdx != CMD_NOT_FOUND && commands[cmdIdx].pCommandFunction != NULL) {
        // execute the related command function, if we have correct number of params
        if(argc == commands[cmdIdx].paramCount + 1) {
            execStatus = (*commands[cmdIdx].pCommandFunction)(argc, argv);
        }
        else {
             printf("  --> Invalid Parameter Count for [%s] %d vs %d\n", argc - 1, commands[cmdIdx].paramCount);
             printf("  USAGE: %s\n\n", commands[cmdIdx].description);
             execStatus = CMD_RET_BAD_PARMS;
        }
    }
    else if(s_nCurrentCmdIdx != CMD_NOT_FOUND && commands[cmdIdx].pCommandFunction == NULL) {
        // show that this command is not yet implemented
        printf("** Command [%s] NOT YET IMPLEMENTED\n", argv[0]);
    }
    else {
        // show that we don't know this command
        printf("** Unknown Command [%s]\n", argv[0]);
        printf("   (enter 'helpcommands' to show full list of commands)\n\n");
    }
    return execStatus;
}

// ----------------------------------------------------------------------------
//  COMMAND Functions
//
int commandAllocBuffers(int argc, const char *argv[])
{
	    int bValidCommand = 1;
    
    // IMPLEMENT:
    //   loadbmpfile {bmpFileName} - load 24-bit bitmap into current buffer
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
	    debugMessage("alloc %d buffers...",nBuffers);
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
                int imageSize;
                struct _BMPColorValue *bitBuffer = loadImageFromFile(fileSpec, &imageSize);
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
//  Missing Functions
//
// REF: https://stackoverflow.com/questions/5820810/case-insensitive-string-comp-in-c
int stricmp(char const *a, char const *b)
{
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
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



