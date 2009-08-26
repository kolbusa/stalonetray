/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * settings.c
 * Sun, 12 Sep 2004 18:55:53 +0700
 * -------------------------------
 * settings parser\container
 * -------------------------------*/

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>

#include "config.h"

#include "common.h"
#include "settings.h"
#include "debug.h"
#include "layout.h"
#include "list.h"
#include "tray.h"

#include "xutils.h"
#include "wmh.h"

/* Here we keep our filthy settings */
struct Settings settings;
/* Initialize data */
void init_default_settings() 
{
	settings.bg_color_str		= "#777777";
	settings.display_str		= NULL;
#ifdef DEBUG
	settings.dbg_level			= 0;
#endif
	settings.geometry_str		= NULL;
	settings.max_geometry_str	= "0x0";
	settings.icon_size			= FALLBACK_ICON_SIZE;
	settings.slot_size          = -1;
	settings.deco_flags			= DECO_NONE;
	settings.max_tray_dims.x	= 0;
	settings.max_tray_dims.y	= 0;
	settings.parent_bg			= 0;
	settings.shrink_back_mode   = 1;
	settings.sticky				= 1;
	settings.skip_taskbar		= 1;
	settings.transparent		= 0;
	settings.vertical			= 0;
	settings.grow_gravity		= GRAV_N | GRAV_W;
	settings.icon_gravity		= GRAV_N | GRAV_W;
	settings.wnd_type			= _NET_WM_WINDOW_TYPE_DOCK;
	settings.wnd_layer			= NULL;
	settings.xsync				= 0;
	settings.need_help			= 0;
	settings.config_fname		= NULL;
	settings.full_pmt_search	= 1;
	settings.min_space_policy	= 0;
	settings.pixmap_bg          = 0;
	settings.bg_pmap_path       = NULL;

	settings.tint_level         = 0;
	settings.tint_color.pixel   = 0xffffff;
	settings.fuzzy_edges        = 0;

	settings.dockapp_mode		= DOCKAPP_NONE;

	settings.scrollbar_size     = 8;
	settings.scrollbar_mode     = SB_MODE_VERT | SB_MODE_HORZ;
	settings.scrollbar_inc		= -1;

	settings.wm_strut_mode		= WM_STRUT_AUTO;

	settings.kludge_flags		= 0;

#ifdef DELAY_EMBEDDING_CONFIRMATION
	settings.confirmation_delay = 3;
#endif
}

/* ******* general parsing utils ********* */

#define PARSING_ERROR(msg,str) if (!silent) ERR(("Parsing error: " msg ", \"%s\" found\n", str));

/* Parse dockapp mode */
int parse_dockapp_mode(char *str, int **tgt, int silent)
{
	if (!strcmp(str, "none"))
		**tgt = DOCKAPP_NONE;
	else if (!strcmp(str, "simple"))
		**tgt = DOCKAPP_SIMPLE;
	else if (!strcmp(str, "wmaker"))
		**tgt = DOCKAPP_WMAKER;
	else {
		PARSING_ERROR("none, simple, or wmaker expected", str);
		return FAILURE;
	}
	return SUCCESS;
}

/* Parse gravity string ORing resulting value
 * with current value of tgt */
int parse_gravity(char *str, int **tgt, int silent)
{
	int i, r = 0, s;

	if (str == NULL) goto fail;

	s = strlen(str);

	if (s > 2) goto fail;
	
	for (i = 0; i < s; i++)
		switch (tolower(str[i])) {
			case 'n': r |= GRAV_N; break;
			case 's': r |= GRAV_S; break;
			case 'w': r |= GRAV_W; break;
			case 'e': r |= GRAV_E; break;
			default: goto fail;
		}

	if ((r & GRAV_N && r & GRAV_S) || (r & GRAV_E && r & GRAV_W)) 
		goto fail;

	**tgt = r;

	return SUCCESS;
fail:
	PARSING_ERROR("gravity expected", str);
	return FAILURE;
}

/* Parse integer string storing resulting value in tgt */
int parse_int(char *str, int **tgt, int silent)
{
	int r;
	char *tail;
	r = strtol(str, &tail, 0);
	if (*tail == 0) {
		**tgt = r;
		return SUCCESS;
	} else {
		PARSING_ERROR("integer expected", str);
		return FAILURE;
	}
}

/* Parse kludges mode */
int parse_kludges(char *str, int **tgt, int silent)
{
	char *curtok = str, *rest = str;
	do {
		if ((rest = strchr(rest, ',')) != NULL) *(rest++) = 0;
		if (!strcasecmp(curtok, "fix_window_pos"))
			**tgt = KLUDGE_FIX_WND_POS;
/*        else if (!strcasecmp(curtok, "fix_window_size"))*/
/*            **tgt = KLUDGE_FIX_WND_SIZE;*/
		else if (!strcasecmp(curtok, "force_icons_size"))
			**tgt = KLUDGE_FORCE_ICONS_SIZE;
		else if (!strcasecmp(curtok, "use_icons_hints"))
			**tgt = KLUDGE_USE_ICONS_HINTS;
		else {
			PARSING_ERROR("kludge flag expected", curtok);
			return FAILURE;
		}
		curtok = rest;
	} while (rest != NULL);
	return SUCCESS;
}

/* Parse strut mode */
int parse_strut_mode(char *str, int **tgt, int silent)
{
	if (!strcasecmp(str, "auto"))
		**tgt = WM_STRUT_AUTO;
	else if (!strcasecmp(str, "top"))
		**tgt = WM_STRUT_TOP;
	else if (!strcasecmp(str, "bottom"))
		**tgt = WM_STRUT_BOT;
	else if (!strcasecmp(str, "left"))
		**tgt = WM_STRUT_LFT;
	else if (!strcasecmp(str, "right"))
		**tgt = WM_STRUT_RHT;
	else if (!strcasecmp(str, "none"))
		**tgt = WM_STRUT_NONE;
	else {
		PARSING_ERROR("one of top, bottom, left, right, or auto expected", str);
		return FAILURE;
	}
	return SUCCESS;
}

/* Parse boolean string storing result in tgt*/
int parse_bool(char *str, int **tgt, int silent)
{
	if (!strcasecmp(str, "yes") || !strcasecmp(str, "on") || !strcasecmp(str, "true") || !strcasecmp(str, "1"))
		**tgt = True;
	else if (!strcasecmp(str, "no") || !strcasecmp(str, "off") || !strcasecmp(str, "false") || !strcasecmp(str, "0"))
		**tgt = False;
	else {
		PARSING_ERROR("boolean expected", str);
		return FAILURE;
	}
	return SUCCESS;
}

/* Backwards version of the boolean parser */
int parse_bool_rev(char *str, int **tgt, int silent)
{
	if (parse_bool(str, tgt, silent)) {
		**tgt = !**tgt;
		return SUCCESS;
	}	
		return FAILURE;
}

/* Parse window layer string storing result in tgt */
int parse_wnd_layer(char *str, char ***tgt, int silent)
{
	if (!strcasecmp(str, "top"))
		**tgt = _NET_WM_STATE_ABOVE;
	else if (!strcasecmp(str, "bottom"))
		**tgt = _NET_WM_STATE_BELOW;
	else if (!strcasecmp(str, "normal"))
		**tgt = NULL;
	else {
		PARSING_ERROR("window layer expected", str);
		return FAILURE;
	}
	return SUCCESS;
}

/* Parse window type string storing result in tgt */
int parse_wnd_type(char *str, char ***tgt, int silent)
{
	if (!strcasecmp(str, "dock"))
		**tgt = _NET_WM_WINDOW_TYPE_DOCK;
	else if (!strcasecmp(str, "toolbar"))
		**tgt = _NET_WM_WINDOW_TYPE_TOOLBAR;
	else if (!strcasecmp(str, "utility"))
		**tgt = _NET_WM_WINDOW_TYPE_UTILITY;
	else if (!strcasecmp(str, "normal"))
		**tgt = _NET_WM_WINDOW_TYPE_NORMAL;
	else if (!strcasecmp(str, "desktop"))
		**tgt = _NET_WM_WINDOW_TYPE_DESKTOP;
	else {
		PARSING_ERROR("window type expected", str);
		return FAILURE;
	}
	return SUCCESS;
}

/* Just copy string from arg to *tgt */
int parse_copystr(char *str, char ***tgt, int silent)
{
	**tgt = strdup(str);
	if (**tgt == NULL) DIE(("Out of memory"));
	return SUCCESS;
}

/* Parses window decoration specification */
int parse_deco(char *str, int **tgt, int silent)
{
	if (!strcasecmp(str, "none"))
		**tgt = DECO_NONE;
	else if (!strcasecmp(str, "all"))
		**tgt = DECO_ALL;
	else if (!strcasecmp(str, "border"))
		**tgt = DECO_BORDER;
	else if (!strcasecmp(str, "title"))
		**tgt = DECO_TITLE;
	else {
		PARSING_ERROR("decoration specification expected", str);
		return FAILURE;
	}
	return SUCCESS;
}

/* Parses window decoration specification */
int parse_sb_mode(char *str, int **tgt, int silent)
{
	if (!strcasecmp(str, "none"))
		**tgt = 0;
	else if (!strcasecmp(str, "vertical"))
		**tgt = SB_MODE_VERT;
	else if (!strcasecmp(str, "horizontal"))
		**tgt = SB_MODE_HORZ;
	else if (!strcasecmp(str, "all"))
		**tgt = SB_MODE_HORZ | SB_MODE_VERT;
	else {
		PARSING_ERROR("scrollbars specification expected", str);
		return FAILURE;
	}
	return SUCCESS;
}

/************ CLI **************/

#define MAX_TARGETS 10

/* parameter parser function */
typedef int (*param_parser_t) (char *str, void *tgt[MAX_TARGETS], int silent);

struct Param {
	char *short_name;		/* Short command line parameter name */
	char *long_name;		/* Long command line parameter name */
	char *rc_name;			/* Parameter name for rc file */

	void *target[MAX_TARGETS];/* Pointers to the values that are set by this parameter */

	param_parser_t parser;	/* Pointer to parsing function */

	int pass;				/* 0th pass parameters are parsed before rc file,
							   1st pass parameters are parsed after it */

	int takes_arg;			/* Wheather this parameter takes an argument */

	int optional_arg; 		/* Wheather the argument is optional */

	char *default_arg_val;	/* Default value of the argument if none is given */

	/* char *desc; */		/* TODO: Description */
};

struct Param params[] = {
	{"-display", NULL, "display", {&settings.display_str}, (param_parser_t) &parse_copystr, 1, 1, 0, NULL},
#ifdef DEBUG
	{NULL, "--dbg-level", "dbg_level", {&settings.dbg_level}, (param_parser_t) &parse_int, 1, 1, 0, NULL},
#endif
	{"-bg", "--background", "background", {&settings.bg_color_str}, (param_parser_t) &parse_copystr, 1, 1, 0, NULL},
	{"-c", "--config", NULL, {&settings.config_fname}, (param_parser_t) &parse_copystr, 0, 1, 0, NULL},
#ifdef DELAY_EMBEDDING_CONFIRMATION
	{NULL, "--confirmation-delay", "confirmation_delay", {&settings.confirmation_delay}, (param_parser_t) &parse_int, 1, 1, 0, NULL},
#endif
	{"-d", "--decorations", "decorations", {&settings.deco_flags}, (param_parser_t) &parse_deco, 1, 1, 1, "all"},
	{NULL, "--dockapp-mode", "dockapp_mode", {&settings.dockapp_mode}, (param_parser_t) &parse_dockapp_mode, 1, 1, 1, "simple"},
	{"-f", "--fuzzy-edges", "fuzzy_edges", {&settings.fuzzy_edges}, (param_parser_t) &parse_int, 1, 1, 1, "2"},
	{"-geometry", "--geometry", "geometry", {&settings.geometry_str}, (param_parser_t) &parse_copystr, 1, 1, 0, NULL},
	{NULL, "--grow-gravity", "grow_gravity", {&settings.grow_gravity}, (param_parser_t) &parse_gravity, 1, 1, 0, NULL},
	{NULL, "--icon-gravity", "icon_gravity", {&settings.icon_gravity}, (param_parser_t) &parse_gravity, 1, 1, 0, NULL},
	{ "-i", "--icon-size", "icon_size", {&settings.icon_size}, (param_parser_t) &parse_int, 1, 1, 0, NULL}, 
	{"-h", "--help", NULL, {&settings.need_help}, (param_parser_t) &parse_bool, 0, 0, 0, "true" },
	{NULL, "--kludges", "kludges", {&settings.kludge_flags}, (param_parser_t) &parse_kludges, 1, 1, 0, NULL},
	{NULL, "--max-geometry", "max_geometry", {&settings.max_geometry_str}, (param_parser_t) &parse_copystr, 1, 1, 0, NULL},
	{NULL, "--no-shrink", "no_shrink", {&settings.shrink_back_mode}, (param_parser_t) &parse_bool_rev, 1, 1, 1, "true"},
	{"-p", "--parent-bg", "parent_bg", {&settings.parent_bg}, (param_parser_t) &parse_bool, 1, 1, 1, "true"},
#ifdef XPM_SUPPORTED
	{NULL, "--pixmap-bg", "pixmap_bg", {&settings.bg_pmap_path}, (param_parser_t) &parse_copystr, 1, 1, 0, NULL},
#endif
	{NULL, "--scrollbars", "scrollbars", {&settings.scrollbar_mode}, (param_parser_t) &parse_sb_mode, 1, 1, 0, "none"},
	{NULL, "--scrollbars-step", "scrollbars_step", {&settings.scrollbar_inc}, (param_parser_t) &parse_int, 1, 1, 0, "8"},
	{NULL, "--skip-taskbar", "skip_taskbar", {&settings.skip_taskbar}, (param_parser_t) &parse_bool, 1, 1, 1, "true"},
	{"-s", "--slot-size", "slot_size", {&settings.slot_size}, (param_parser_t) &parse_int, 1, 1, 0, NULL},
	{NULL, "--sticky", "sticky", {&settings.sticky}, (param_parser_t) &parse_bool, 1, 1, 1, "true"},
	{NULL, "--tint-color", "tint_color", {&settings.tint_color_str}, (param_parser_t) &parse_copystr, 1, 1, 0, NULL},
	{NULL, "--tint-level", "tint_level", {&settings.tint_level}, (param_parser_t) &parse_int, 1, 1, 0, NULL},
	{"-t", "--transparent", "transparent", {&settings.transparent}, (param_parser_t) &parse_bool, 1, 1, 1, "true"},
	{"-v", "--vertical", "vertical", {&settings.vertical}, (param_parser_t) &parse_bool, 1, 1, 1, "true"},
	{NULL, "--window-layer", "window_layer", {&settings.wnd_layer}, (param_parser_t) &parse_wnd_layer, 1, 1, 1, NULL},
	{NULL, "--window-strut", "window_strut", {&settings.wm_strut_mode}, (param_parser_t) &parse_strut_mode, 1, 1, 1, NULL},
	{NULL, "--window-type", "window_type", {&settings.wnd_type}, (param_parser_t) &parse_wnd_type, 1, 1, 1, NULL},
	{NULL, "--xsync", "xsync", {&settings.xsync}, (param_parser_t) &parse_bool, 1, 1, 1, "true"},
	{NULL, NULL, NULL, {NULL}}
};

void usage(char *progname) 
{
	printf( "\nUsage: %s [options...]\n", progname);
	printf( "\n"
			"For short options argument can be specified as -o value or -ovalue.\n"
			"For long options argument can be specified as --option value or\n"
			"--option=value. All flag-options have expilicit optional boolean \n"
			"argument, which can be true (1, yes) or false (0, no).\n"
			"\n"
			"Possible options are:\n"
			"    -display <display>          use X display <display>\n"
#ifdef DEBUG
			"    --dbg-level <n>             set the level of debug output to <n>\n"
			"                                (defalt: 0)\n"
#endif
			"    -bg, --background <color>   select background color (default: #777777)\n"
			"    -c, --config <filename>     read configuration from <file>\n"
			"                                (instead of default $HOME/.stalonetrayrc)\n"
			"    -d, --decorations [<deco>]  set what part of window decorations are\n"
			"                                visible; deco can be: none (default),\n"
			"                                title, border, all\n"
			"    --dockapp-mode [<mode>]     enable dockapp mode; mode can be none (default),\n"
			"                                simple (default if no mode specified), or wmaker\n"
			"    -f, --fuzzy-edges [<level>] set edges fuzziness, level is from\n"
			"                                0 (disabled) to 3 (maximum); works with\n"
			"                                tinting and/or pixmap background\n"
			"    [-]-geometry <geometry>     set initial tray`s geometry (width and height\n"
			"                                are defined in icon slots; offsets are defined\n"
			"                                in pixels)\n"
			"    --grow-gravity <gravity>    tray`s grow gravity,\n"
			"                                one of N, S, W, E, NW, NE, SW, SE\n"
			"    --icon-gravity <gravity>    icon positioning gravity (NW, NE, SW, SE)\n"
			"    -i, --icon-size <n>         set basic icon size to <n>, default: 24\n"
			"    -h, --help                  show this message\n"
			"    --kludges <list>            enable specific kludges for non-conforming WMs\n"
			"                                and/or stalonetray bugs; argument is a comma\n"
			"                                separated list of:\n"
			"                                 - fix_window_pos (fix window position),\n"
			"                                 - force_icons_size (ignore icon resizes),\n"
			"                                 - use_icons_hints (use icon size hints)\n"
			"    --max-geometry <geometry>   set tray maximal width and height; 0 indicates\n"
			"                                no limit in respective direction\n"
			"    --no-shrink                 do not shrink window back after icon removal\n"
			"    -p, --parent-bg             use parent for background\n"
			"    --pixmap-bg <pixmap>        use pixmap for tray`s window background\n"
			"    --respect-icon-hints        try to respect icon size hints\n"
			"    --scrollbars <mode>         set scrollbar mode, one of: all, horizontal,\n"
			"                                vertical, none\n"
			"    --scrollbars-step <n>       set scrollbar step to n pixels\n"
			"    --slot-size <n>             set icon slot size to n\n"
			"    --skip-taskbar              hide tray`s window from the taskbar\n"
			"    --sticky                    make tray`s window sticky across multiple\n"
			"                                desktops/pages\n"
			"    --tint-color <color>        tint tray background with color (not used\n"
			"                                with plain color background)\n"
			"    --tint-level <level>        set tinting level from 0 to 255\n"
			"    -t, --transparent           enable root transparency\n"
			"    -v, --vertical              use vertical layout of icons (horizontal\n"
			"                                layout is used by default)\n"
			"    --window-layer <layer>      set tray`s window EWMH layer, one of:\n"
			"                                bottom, normal, top\n"
			"    --window-strut <mode>       set window strut mode, one of: auto,\n"
			"                                left, right, top, bottom\n"
			"    --window-type <type>        set tray`s window EWMH type, one of:\n"
			"                                normal, dock, toolbar, utility, desktop\n"
			"    --xsync                     operate on X server synchronously (SLOW)\n"
			"\n"
			);
}

/* Parse command line parameters */
int parse_cmdline(int argc, char **argv, int pass)
{
	struct Param *p, *match;
	char *arg, *progname = argv[0];
	while (--argc > 0) {
		argv++;
		match = NULL;
		for (p = params; p->parser != NULL; p++) {
			if (p->takes_arg) {
				if (p->short_name != NULL && strstr(*argv, p->short_name) == *argv) {
					if ((*argv)[strlen(p->short_name)] != '\0') /* accept arguments in the form -a5 */
						arg = *argv + strlen(p->short_name);
					else if (argc > 1 && argv[1][0] != '-') { /* accept arguments in the form -a 5,
																 do not accept values starting with '-' */
																 
						arg = *(++argv);
						argc--;
					} else if(!p->optional_arg) { /* argument is missing */
						ERR(("%s expects an argument\n", p->short_name));
						break;
					} else /* argument is optional, use default value */
							arg = p->default_arg_val;
				} else if (p->long_name != NULL && strstr(*argv, p->long_name) == *argv) {
					if ((*argv)[strlen(p->long_name)] == '=') /* accept arguments in the form --abcd=5 */
						arg = *argv + strlen(p->long_name) + 1;
					else if ((*argv)[strlen(p->long_name)] == '\0') { /* accept arguments in the from ---abcd 5 */
						if (argc > 1 && argv[1][0] != '-' ) { /* arguments cannot start with the dash */
							arg = *(++argv); 
							argc--;
						} else if (!p->optional_arg) { /*argument is missing */
							ERR(("%s expects an argument\n", p->long_name));
							break;
						} else /* argument is optional, use default value */
							arg = p->default_arg_val;
					} else continue; /* just in case when there can be both --abc and --abcd */
				} else continue;
				match = p;
				break;
			} else if (strcmp(*argv, p->short_name) == 0 || strcmp(*argv, p->long_name) == 0) {
				match = p;
				arg = p->default_arg_val;
				break;
			}
		}
		
#define USAGE_AND_DIE() do { usage(progname); DIE(("Could not parse command line\n")); } while (0)

		if (match == NULL) USAGE_AND_DIE();
		
		if (match->pass != pass) continue;
		
		assert(arg != NULL);

		DBG(4, ("cmdline: pass %d, param \"%s\", arg \"%s\"\n", pass, match->long_name != NULL ? match->long_name : match->short_name, arg));

		if (!match->parser(arg, match->target, match->optional_arg)) {
			if (match->optional_arg) {
				argc++; argv--;
				assert(arg != match->default_arg_val);
				match->parser(match->default_arg_val, match->target, False);
			} else
				USAGE_AND_DIE();
		}

	}

	if (settings.need_help) {
		usage(progname);
		exit(0);
	}

	return SUCCESS;
}

/************ .stalonetrayrc ************/

/**************************************************************************************
 * <line> ::= [<whitespaces>] [(<arg> <whitespaces>)* <arg>] [<whitespaces>] <comment>
 * <arg> ::= "<arbitrary-text>"|<text-without-spaces-and-#>
 * <comment> ::= # <arbitrary-text>
 **************************************************************************************/

/* Does exactly what its name says */
#define SKIP_SPACES(p) { for (; *p != 0 && isspace((int) *p); p++); }
/* Break the line in argc, argv pair */
int get_args(char *line, int *argc, char ***argv)
{
	int q_flag = 0;
	char *arg_start, *q_pos;
	
	*argc = 0;
	*argv = NULL;
	
	/* 1. Strip leading spaces */
	SKIP_SPACES(line);
	if (0 == *line) { /* meaningless line */
		return SUCCESS;
	}
	arg_start = line;

	/* 2. Strip comments */
	for (; 0 != *line; line++) {
		q_flag = ('"' == *line) ? !q_flag : q_flag;
		if ('#' == *line && !q_flag) {
			*line = 0;
			break;
		}
	}
	if (q_flag) { /* disbalance of quotes */
		ERR(("Disbalance of quotes\n"));
		return FAILURE;
	}
	if (arg_start == line) { /* meaningless line */
		return SUCCESS;
	}
	line--;
	
	/* 3. Strip trailing spaces */	
	for (; line != arg_start && isspace((int) *line); line--);
	if (arg_start == line) { /* meaningless line */
		return FAILURE;
	}
	*(line + 1) = 0; /* this _is_ really ok since isspace(0) != 0 */
	line = arg_start;
	
	/* 4. Extract arguments */
	do {
		(*argc)++;
		if (NULL == (*argv = realloc(*argv, *argc * sizeof(char *)))) {
			free(*argv);
			DIE(("Out of memory\n"));
		}
		
		if (*arg_start == '"') { /* 4.1. argument is quoted: find matching quote */
			arg_start++;
			(*argv)[*argc - 1] = arg_start;
			if (NULL == (q_pos = strchr(arg_start, '"'))) {
				free(*argv);
				DIE(("Internal error: quotes balance calculation failed"));
				return FAILURE;
			}
			arg_start = q_pos;
		} else { /* 4.2. whitespace-separated argument: find fist whitespace */
			(*argv)[*argc - 1] = arg_start;
			for (; 0 != *arg_start && !isspace((int) *arg_start); arg_start++);
		}

		if (*arg_start != 0) {
			*arg_start = 0;
			arg_start++;
			SKIP_SPACES(arg_start);
		}
	} while(*arg_start != 0);
	
	return SUCCESS;
}

#define READ_BUF_SZ	512
/* Parses rc file (path is either taken from settings.config_fname
 * or ~/.stalonetrayrc is used) */
void parse_rc()
{
	char *home_dir;
	static char config_fname[PATH_MAX];
	FILE *cfg;

	char buf[READ_BUF_SZ + 1];
	int lnum = 0;

	int argc;
	char **argv, *arg;

	struct Param *p, *match;

	/* 1. Setup file name */
	if (settings.config_fname == NULL) {
		if ((home_dir = getenv("HOME")) == NULL) {
			ERR(("You have no $HOME. I'm sorry for you.\n"));
			return;
		}
		snprintf(config_fname, PATH_MAX-1, "%s/%s", home_dir, STALONETRAY_RC);
		settings.config_fname = config_fname;
	}

	DBG(3, ("using config file \"%s\"\n", settings.config_fname));

	/* 2. Open file */
	cfg = fopen(settings.config_fname, "r");
	if (cfg == NULL) {
		ERR(("could not open %s (%s)\n", settings.config_fname, strerror(errno)));
		return;
	}

	/* 3. Read the file line by line */
	buf[READ_BUF_SZ] = 0;
	while (!feof(cfg)) {
		lnum++;
		if (fgets(buf, READ_BUF_SZ, cfg) == NULL) {
			if (ferror(cfg)) ERR(("read error (%s)\n", strerror(errno)));
			break;
		}

		if (!get_args(buf, &argc, &argv)) {
			DIE(("rc file parse error at %s:%d: could not parse line\n", settings.config_fname, lnum));
		}
		if (!argc) continue; /* This is empty/comment-only line */

		match = NULL;
		for (p = params; p->parser != NULL; p++) {
			if (p->rc_name != NULL && strcmp(argv[0], p->rc_name) == 0) {
				if (argc - 1 > (p->takes_arg ? 1 : 0) || (!p->optional_arg && argc - 1 < 1)) 
					DIE(("rc file parse error at %s:%d:" 
								"invalid number of args for \"%s\" (%s required)\n", 
								settings.config_fname, 
								lnum, 
								argv[0], 
								p->optional_arg ? "0/1" : "1" )); 
				match = p;
				arg = (!p->takes_arg || (p->optional_arg && argc == 1)) ? p->default_arg_val : argv[1];
				break;
			}
		}
		if (!match) {
			DIE(("rc file parse error at %s:%d: unrecognized rc file keyword \"%s\".\n", settings.config_fname, lnum, argv[0]));
		}
		assert(arg != NULL);
		DBG(4, ("rc: param \"%s\", arg \"%s\"\n", match->rc_name, arg));
		if (!match->parser(arg, match->target, False)) {
			DIE(("Rc file parse error at %s:%d: could not parse argument for \"%s\".\n", settings.config_fname, lnum, argv[0]));
		}

	}
}

/* Interpret all settings that need an open display */
void interpret_settings()
{
	static int gravity_matrix[11] = {
		ForgetGravity, 
		EastGravity,
		WestGravity,
		ForgetGravity,
		SouthGravity,
		SouthEastGravity,
		SouthWestGravity,
		ForgetGravity,
		NorthGravity,
		NorthEastGravity,
		NorthWestGravity
	};
	int geom_flags; 
	int rc;
	int dummy;
	XWindowAttributes root_wa;

	/* Sanitize icon size */
	val_range(settings.icon_size, MIN_ICON_SIZE, INT_MAX);
	if (settings.slot_size < settings.icon_size) settings.slot_size = settings.icon_size;
	if (settings.scrollbar_inc < settings.slot_size / 2) settings.scrollbar_inc = settings.slot_size / 2;

	/* Sanitize all gravity strings */
	settings.icon_gravity |= ((settings.icon_gravity & GRAV_V) ? 0 : GRAV_N);
	settings.icon_gravity |= ((settings.icon_gravity & GRAV_H) ? 0 : GRAV_W);

	settings.win_gravity = gravity_matrix[settings.grow_gravity];
	settings.bit_gravity = gravity_matrix[settings.icon_gravity];

	/* Parse all background-related settings */
#ifdef XPM_SUPPORTED
	settings.pixmap_bg = (settings.bg_pmap_path != NULL);
#endif

	if (settings.pixmap_bg) {
		settings.parent_bg = False;
		settings.transparent = False;
	}

	if (settings.transparent)
		settings.parent_bg = False;

	/* Parse background color */
	rc = XParseColor(tray_data.dpy, XDefaultColormap(tray_data.dpy, DefaultScreen(tray_data.dpy)),
			settings.bg_color_str, &settings.bg_color);
	if (rc) 
		XAllocColor(tray_data.dpy, XDefaultColormap(tray_data.dpy, DefaultScreen(tray_data.dpy)), 
				&settings.bg_color);
	
	if (!x11_ok() || !rc)
		DIE(("could not parse background color \"%s\"\n", settings.bg_color_str));

	/* Sanitize tint level value */
	val_range(settings.tint_level, 0, 255);

	if (settings.tint_level) {
		/* Parse tint color */
		rc = XParseColor(tray_data.dpy, XDefaultColormap(tray_data.dpy, DefaultScreen(tray_data.dpy)),
				settings.tint_color_str, &settings.tint_color);
		if (rc) 
			XAllocColor(tray_data.dpy, XDefaultColormap(tray_data.dpy, DefaultScreen(tray_data.dpy)), 
					&settings.tint_color);

		if (!x11_ok() || !rc) 
			DIE(("could not parse tint color \"%s\"\n", settings.bg_color_str));
	}

	/* Sanitize edges fuzziness */
	val_range(settings.fuzzy_edges, 0, 3);

	/* Get dimensions of root window */
	if (XGetWindowAttributes(tray_data.dpy, DefaultRootWindow(tray_data.dpy), &root_wa))
		DBG(6, ("root window geometry %dx%d\n", root_wa.width, root_wa.height));
	else
		DIE(("could not get root window dimensions. WEIRD...\n"));

	tray_data.root_wnd.x = 0;
	tray_data.root_wnd.y = 0;
	tray_data.root_wnd.width = root_wa.width;
	tray_data.root_wnd.height = root_wa.height;

	/* Set tray maximal width/height */
#if 1
	geom_flags = XParseGeometry(settings.max_geometry_str,
					&dummy, &dummy,
					(unsigned int *) &settings.max_tray_dims.x,
					(unsigned int *) &settings.max_tray_dims.y);
	DBG(9, ("max geometry from max_geometry_str: %dx%d\n",
					settings.max_tray_dims.x,
					settings.max_tray_dims.y));
	settings.max_tray_dims.x *= settings.slot_size;
	settings.max_tray_dims.y *= settings.slot_size;
	if (!settings.max_tray_dims.x)
		settings.max_tray_dims.x = root_wa.width;
	else
		val_range(settings.max_tray_dims.x, settings.slot_size, INT_MAX);
	
	if (!settings.max_tray_dims.y)
		settings.max_tray_dims.y = root_wa.height;
	else
		val_range(settings.max_tray_dims.y, settings.slot_size, INT_MAX);
#else
	if (!settings.max_tray_width)
		settings.max_tray_width = root_wa.width / settings.slot_size;
	else 
		val_range(settings.max_tray_width, 1, INT_MAX);
	
	if (!settings.max_tray_height)
		settings.max_tray_height = root_wa.height / settings.slot_size;
	else
		val_range(settings.max_tray_height, 1, INT_MAX);
#endif
	DBG(9, ("max geometry after normalization: %dx%d\n",
					settings.max_tray_dims.x,
					settings.max_tray_dims.y));

#	define DEFAULT_GEOMETRY "1x1+0+0"
	tray_data.xsh.flags = PResizeInc | PBaseSize;
	tray_data.xsh.x = 0;
	tray_data.xsh.y = 0;
	tray_data.xsh.width_inc = settings.slot_size;
	tray_data.xsh.height_inc = settings.slot_size;
	tray_data.xsh.base_width = 0;
	tray_data.xsh.base_height = 0;
	tray_calc_window_size(0, 0, &tray_data.xsh.base_width, &tray_data.xsh.base_height);
	geom_flags = XWMGeometry(tray_data.dpy, DefaultScreen(tray_data.dpy),
			settings.geometry_str, DEFAULT_GEOMETRY, 0,
			&tray_data.xsh, &tray_data.xsh.x, &tray_data.xsh.y,
			&tray_data.xsh.width, &tray_data.xsh.height, &tray_data.xsh.win_gravity);
	tray_data.xsh.win_gravity = settings.win_gravity;
	tray_data.xsh.min_width = tray_data.xsh.width;
	tray_data.xsh.min_height = tray_data.xsh.height;
	tray_data.xsh.max_width = tray_data.xsh.width;
	tray_data.xsh.min_height = tray_data.xsh.height;
	tray_data.xsh.flags = PResizeInc | PBaseSize | PMinSize | PMaxSize | PWinGravity; 

	settings.orig_tray_dims.x = tray_data.xsh.width;
	settings.orig_tray_dims.y = tray_data.xsh.height;

	if (settings.dockapp_mode == DOCKAPP_WMAKER) 
		tray_data.xsh.flags |= USPosition; 
	else {
		if (geom_flags & (XValue | YValue)) tray_data.xsh.flags |= USPosition; else tray_data.xsh.flags |= PPosition;
		if (geom_flags & (WidthValue | HeightValue)) tray_data.xsh.flags |= USSize; else tray_data.xsh.flags |= PSize;
	}
	DBG(3, ("final geometry: %dx%d at (%d,%d)\n", 
			tray_data.xsh.width, tray_data.xsh.height,
			tray_data.xsh.x, tray_data.xsh.y));

	if ((geom_flags & XNegative) && (geom_flags & YNegative)) 
		settings.geom_gravity = SouthEastGravity;
	else if (geom_flags & YNegative) 
		settings.geom_gravity = SouthWestGravity;
	else if (geom_flags & XNegative) 
		settings.geom_gravity = NorthEastGravity;
	else
		settings.geom_gravity = NorthWestGravity;

	/* XXX: this assumes certain degree of symmetry and in some point 
	 * in the future this may not be the case... */
	tray_calc_window_size(0, 0, &tray_data.scrollbars_data.scroll_base.x, &tray_data.scrollbars_data.scroll_base.y);
	tray_data.scrollbars_data.scroll_base.x /= 2;
	tray_data.scrollbars_data.scroll_base.y /= 2;
}

/************** "main" ***********/
#define DUMP_SETTINGS
int read_settings(int argc, char **argv) 
{
	ERR(("stalonetray "VERSION" [ " FEATURE_LIST " ] starting\n"));
	init_default_settings();
	/* Parse 0th pass command line args */
	parse_cmdline(argc, argv, 0);
	/* Parse configuration files */
	parse_rc();
	/* Parse 1st pass command line args */
	parse_cmdline(argc, argv, 1);
#ifdef DUMP_SETTINGS
	DBG(3, ("--flags--\n"));
	DBG(3, ("parent_bg = %d\n", settings.parent_bg));
	DBG(3, ("deco_flags = 0x%x\n", settings.deco_flags));
	DBG(3, ("min_space_policy = %d\n", settings.min_space_policy));
	DBG(3, ("full_pmt_search = %d\n", settings.full_pmt_search));
	DBG(3, ("shrink_back_mode = %d\n", settings.shrink_back_mode));
	DBG(3, ("need_help = %d\n", settings.need_help));
	DBG(3, ("xsync = %d\n", settings.xsync));
	DBG(3, ("vertical = %d\n", settings.vertical));
	DBG(3, ("dockapp_mode = %d\n", settings.dockapp_mode));
	DBG(3, ("--strings--\n"));
	DBG(3, ("display_str = \"%s\"\n", settings.display_str));
	DBG(3, ("bg_color_str = \"%s\"\n", settings.bg_color_str));
	DBG(3, ("geometry_str = \"%s\"\n", settings.geometry_str));
	DBG(3, ("config_fname = \"%s\"\n", settings.config_fname));
	DBG(3, ("--values--\n"));
	DBG(3, ("icon_size = %d\n", settings.icon_size));
	DBG(3, ("slot_size = %d\n", settings.slot_size));
	DBG(3, ("grow_gravity = 0x%x\n", settings.grow_gravity));
	DBG(3, ("icon_gravity = 0x%x\n", settings.icon_gravity));
	DBG(3, ("max_tray_dims.x = %d\n", settings.max_tray_dims.x));
	DBG(3, ("max_tray_dims.y = %d\n", settings.max_tray_dims.y));
	DBG(3, ("dbg_level = %d\n", settings.dbg_level));
#endif
	return SUCCESS;
}
