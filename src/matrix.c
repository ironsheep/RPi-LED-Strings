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
#include <sys/types.h>
#include <argp.h>

static error_t parse_opt (int key, char *arg, struct argp_state *state);
static void show_version (FILE *stream, struct argp_state *state);

/* argp option keys */
enum {
    DUMMY_KEY=129,
    DRYRUN_KEY
};

/* Option flags and variables.  These are initialized in parse_opt.  */

int want_quiet;			/* --quiet, --silent */
int want_verbose;		/* --verbose */
int want_dry_run;		/* --dry-run */

static struct argp_option options[] =
{
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

int
main (int argc, char **argv)
{
	textdomain(PACKAGE);
	argp_parse(&argp, argc, argv, 0, NULL, NULL);

	/* TODO: do the work */
	processCommands();

	exit (0);
}

/* Parse a single option.  */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case ARGP_KEY_INIT:
      /* Set up default values.  */
      want_quiet = 0;
      want_verbose = 0;
      want_dry_run = 0;
      break;

    case 'q':			/* --quiet, --silent */
      want_quiet = 1;
      break;
    case 'v':			/* --verbose */
      want_verbose = 1;
      break;
    case DRYRUN_KEY:		/* --dry-run */
      want_dry_run = 1;
      break;

    case ARGP_KEY_ARG:		/* [FILE]... */
      /* TODO: Do something with ARG, or remove this case and make
         main give argp_parse a non-NULL fifth argument.  */
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Show the version number and copyright information.  */
static void
show_version (FILE *stream, struct argp_state *state)
{
  (void) state;
  /* Print in small parts whose localizations can hopefully be copied
     from other programs.  */
  fputs(PACKAGE" "VERSION"\n", stream);
  fprintf(stream, _("Written by %s.\n\n"), "Stephen M Moraco");
  fprintf(stream, _("Copyright (C) %s %s\n"), "2019", "Stephen M Moraco");
  fputs(_("\
This program is free software; you may redistribute it under the terms of\n\
the GNU General Public License.  This program has absolutely no warranty.\n"),
	stream);
}
