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
#include <argp.h>
#include <string.h>     // for strxxx()
#include <libgen.h>     // for basename(), dirname()

#include "debug.h"
#include "matrixDriver.h"

#include <sys/types.h> // this must be before system.h
#include "system.h"

#include "commandProcessor.h"

#define EXIT_FAILURE 1

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# define textdomain(Domain)
# define _(Text) Text
#endif
#define N_(Text) Text

static error_t parse_opt (int key, char *arg, struct argp_state *state);
static void show_version (FILE *stream, struct argp_state *state);

/* argp option keys */
enum {
    DUMMY_KEY=129,
    DRYRUN_KEY
};

/* Option flags and variables.  These are initialized in parse_opt.  */

int want_quiet;			/* --quiet, --silent */
int want_dry_run;		/* --dry-run */
char *fileName;		/* final argument: filename when provided */

static struct argp_option options[] =
{
  { "debug",       'd',           NULL,            0,
    N_("Print DEBUG information"), 0 },
  { "quiet",       'q',           NULL,            0,
    N_("Inhibit usual output"), 0 },
  { "silent",      0,             NULL,            OPTION_ALIAS,
    NULL, 0 },
  { "verbose",     'v',           NULL,            0,
    N_("Print more information"), 0 },
  { "dry-run",     DRYRUN_KEY,    NULL,            0,
    N_("Take no real actions"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* The argp functions examine these global variables.  */
const char *argp_program_bug_address = "<stephen@ironsheep.biz>";
void (*argp_program_version_hook) (FILE *, struct argp_state *) = show_version;

static struct argp argp =
{
  options, parse_opt, N_("[FILE...]"),
  N_("interactive LED Matrix console"),
  NULL, NULL, NULL
};

static const char *parameters[2] = { "help", NULL };

int main (int argc, char **argv)
{
    int fd;
    int status;
    int argCt;
    int paramCt;

    debugMessage("main() ENTRY");

    debugMessage("argc=(%d)", argc);
    for(int argCt=0; argCt<argc; argCt++) {
    	debugMessage("arg[%d] = [%s]", argCt, argv[argCt]);
    }
    
    textdomain(PACKAGE);


    //debugMessage("- copy app name");
    
    
    // save off our appname
    char *tmpStr = strdup((const char *)basename(argv[0]));
    if(tmpStr != NULL) {
    	pAppName = tmpStr;
    }

    //debugMessage("parse args");
    
    argp_parse(&argp, argc, argv, 0, NULL, NULL);

    verboseMessage("open driver");
    
    
    // FIXME: UNDONE only open device if we need it!
    if(!openMatrix()) {
        errorMessage("Failed to connect to driver: LEDfifoLKM Loaded?");
        exit(-1);
    }
    verboseMessage("process commands");

	paramCt = 0;
	if(fileName != NULL) {
		parameters[0] = "load";
		parameters[1] = fileName;
		paramCt = 2;
	}
        
 	
	processCommands( paramCt, parameters );

    verboseMessage("close driver");
	
    if(!closeMatrix()) {
        errorMessage("Failed to disconnect from LEDfifoLKM driver!");
        exit(-1);
    }
    debugMessage("main() EXIT");

	exit (0);
}

/* Parse a single option.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case ARGP_KEY_INIT:
      /* Set up default values.  */
      want_quiet = 0;
      bVerboseEnabled = 0;
      want_dry_run = 0;
      bDebugEnabled = 0;
      break;

    case 'd':			/* --debug */
      bDebugEnabled = 1;
      break;
    case 'q':			/* --quiet, --silent */
      want_quiet = 1;
      break;
    case 'v':			/* --verbose */
      bVerboseEnabled = 1;
      break;
    case DRYRUN_KEY:		/* --dry-run */
      want_dry_run = 1;
      break;

    case ARGP_KEY_ARG:		/* [FILE]... */
      if(state->arg_num > 1) {
	      //  Too many Arguments
	      argp_usage(state);
      }
      fileName = arg;
      break;

    case ARGP_KEY_END:
      //if(state->arg_num < 2) {
	      //  NOT Enough Arguments
	      //argp_usage(state);
      //}
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Show the version number and copyright information.  */
static void show_version (FILE *stream, struct argp_state *state)
{
  (void) state;
  /* Print in small parts whose localizations can hopefully be copied
     from other programs.  */
  fputs(PACKAGE" v"VERSION"\n", stream);
  fprintf(stream, _("Written by %s.\n\n"), "Stephen M Moraco");
  fprintf(stream, _("Copyright (C) %s %s\n"), "2019", "Stephen M Moraco");
  fputs(_("\
This program is free software; you may redistribute it under the terms of\n\
the GNU General Public License.  This program has absolutely no warranty.\n"),
	stream);
}
