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
#include <string.h> // strtok()

#include "commandProcessor.h"

char *lsh_read_line(void);
const char **lsh_split_line(char *line, int *tokenCtOut);
int perform(int argc, const char *argv[]);




void processCommands(int argc, const char *argv[])
{
  char *line;
  const char **args;
  int argCt;
  int status;

    printf("- argc=(%d)\n", argc);
    for(int argCt=0; argCt<argc; argCt++) {
	printf("- arg[%d] = [%s]\n", argCt, argv[argCt]);
    }

    // if argc > 0 then do commands and return
    // else  go "interactive"!
    if(argc > 0) {
    }
    else {

  do {
    printf("matrix> ");
    line = lsh_read_line();
    args = lsh_split_line(line, &argCt);
    status = perform(argCt, args);

    free(line);
    free(args);
  } while (status);
    }
}

int perform(int argc, const char *argv[])
{
    printf("- argc=(%d)\n", argc);
    for(int argCt=0; argCt<argc; argCt++) {
	printf("- arg[%d] = [%s]\n", argCt, argv[argCt]);
    }
}

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
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
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



