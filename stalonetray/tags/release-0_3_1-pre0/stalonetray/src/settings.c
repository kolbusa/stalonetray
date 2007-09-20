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
#include <string.h>
#include <wordexp.h>

#include "config.h"
#include "settings.h"
#include "debug.h"
#include "layout.h"
#include "list.h"
#include "tray.h"

#include "xutils.h"
#include "wmh.h"

struct Settings settings = {
/*   configurable via rc file */
	.bg_color_str		= "#777777",
	.display_str		= NULL,
#ifdef DEBUG
	.dbg_level			= 0,
#endif
	.geometry_str		= "120x24+0-0",
	.grow_gravity_str	= "NE",
	.gravity_str		= "NE",
	.icon_size			= FALLBACK_SIZE,
	.hide_wnd_deco		= 0,
	.hide_wnd_title		= 0,
	.hide_wnd_border	= 0,
	.max_tray_width		= 0,
	.max_tray_height	= 0,
	.parent_bg			= 0,
	.permanent			= 1,
	.sticky				= 1,
	.skip_taskbar		= 1,
	.transparent		= 0,
	.vertical			= 0,
	.wnd_type			= _NET_WM_WINDOW_TYPE_DOCK,
	.wnd_layer			= NULL,
	.xsync				= 0,
/*   not configurable via rc file */
	.need_help			= 0,
	.config_fname		= STALONETRAY_RC
};

/* ******* general parsing utils ********* */

int parse_gravity(char *str, int def)
{
	int i, r = 0, s;

	if (str == NULL) {
		return def;
	}

	s = strlen(str) > 2 ? 2 : strlen(str);

	for (i = 0; i < s; i++) {
		switch (str[i]) {
			case 'n':
			case 'N':
				r |= GRAV_N;
				break;
			case 's':
			case 'S':
				r |= GRAV_S;
				break;
			case 'w':
			case 'W':
				r |= GRAV_W;
				break;
			case 'e':
			case 'E':
				r |= GRAV_E;
				break;
			default:
				break;
		}
	}

	if (r & GRAV_N && r & GRAV_S) {
		r = (r & !GRAV_V) | (def | GRAV_V);
	}
	
	if (r & GRAV_E && r & GRAV_W) {
		r = (r & !GRAV_H) | (def | GRAV_H);
	}

	return r;
}

void parse_int(char *str, int *target)
{
	int r;
	char *tail;

	r = strtol(str, &tail, 0);
	if (*tail == 0) *target = r;
}

int parse_bool(char *str, int def)
{
	if (!strcasecmp(str, "yes") || !strcasecmp(str, "on") || !strcasecmp(str, "true") || !strcasecmp(str, "1")) {
		return True;
	}
	
	if (!strcasecmp(str, "no") || !strcasecmp(str, "off") || !strcasecmp(str, "false") || !strcasecmp(str, "0")) {
		return False;
	}

	return def;
}

char *parse_wnd_layer(char *str, char *def)
{
	if (!strcasecmp(str, "top"))
		return _NET_WM_STATE_ABOVE;
	else if (!strcasecmp(str, "bottom"))
		return _NET_WM_STATE_BELOW;
	else
		return def;
}

char *parse_wnd_type(char *str, char *def)
{
	if (!strcasecmp(str, "dock"))
		return _NET_WM_WINDOW_TYPE_DOCK;
	else if (!strcasecmp(str, "toolbar"))
		return _NET_WM_WINDOW_TYPE_TOOLBAR;
	else if (!strcasecmp(str, "utility"))
		return _NET_WM_WINDOW_TYPE_UTILITY;
	else if (!strcasecmp(str, "normal"))
		return _NET_WM_WINDOW_TYPE_NORMAL;
}

/************ CLI **************/

#define CLP_STRING	1
#define CLP_INT		2
#define CLP_FLAG	3

struct CmdLineParam {
	char	*prm_long;
	char	*prm_shrt;

	void	*target;
	
	int		type;
	int		preliminary;
};

#ifdef DEBUG
#define NPARAMS	22
#else
#define NPARAMS	21
#endif

struct CmdLineParam params[NPARAMS] = {
	{"-display", NULL, &settings.display_str, CLP_STRING, 1},
#ifdef DEBUG
	{"--dbg-level", NULL, &settings.dbg_level, CLP_INT, 1},
#endif	
	{"--background", "-bg", &settings.bg_color_str, CLP_STRING, 0},
	{"--config", "-c", &settings.config_fname, CLP_STRING, 1},
	{"--max-width", NULL, &settings.max_tray_width, CLP_INT, 0},
	{"--max-height", NULL, &settings.max_tray_height, CLP_INT, 0},
	{"-geometry", NULL, &settings.geometry_str, CLP_STRING, 0},
	{"--gravity", NULL, &settings.gravity_str, CLP_STRING, 0},
	{"--grow-gravity", NULL, &settings.grow_gravity_str, CLP_STRING, 0},
	{"--icon-size", "-i", &settings.icon_size, CLP_INT, 0},
	{"--help", "-h", &settings.need_help, CLP_FLAG, 1},
	{"--no-deco", "-n", &settings.hide_wnd_deco, CLP_FLAG, 0},
	{"--no-title", NULL, &settings.hide_wnd_title, CLP_FLAG, 0},
	{"--no-border", NULL, &settings.hide_wnd_border, CLP_FLAG, 0},
	{"--parent-bg", "-p", &settings.parent_bg, CLP_FLAG, 0},
	{"--permanent", NULL, &settings.permanent, CLP_FLAG, 0},
	{"--skip-taskbar", NULL, &settings.skip_taskbar, CLP_FLAG, 0},
	{"--sticky", NULL, &settings.sticky, CLP_FLAG, 0},
	{"--no-deco", "-n", &settings.hide_wnd_deco, CLP_FLAG, 0},
	{"--transparent", "-t", &settings.transparent, CLP_FLAG, 0},
	{"--vertical", "-v", &settings.vertical, CLP_FLAG, 0},
	{"--xsync", NULL, &settings.xsync, CLP_FLAG, 0}
};

void usage(char *progname) {
	printf("\nUsage: %s [args]\n\nPossible args are:\n"
			"    -display <display>         use X display <display>\n"
#ifdef DEBUG
			"    --dbg-level <n>            set the level of debug output to <n>\n"
#endif
			"    -bg, --background <color>  select background color\n"
			"    -c, --config <filename>    read configuration from <file>\n"
			"                               (instead of default ~/.stalonetrayrc)\n"
			"    --max-width <n>            set tray`s width limit to <n>\n"
			"                               (default: 0 = unlimited)\n"
			"    --max-height <n>           set tray`s height limit to <n>\n"
			"                               (default: 0 = unlimited)\n"
			"    -geometry <geometry>       use <geometry> as initial tray`s geometry\n"
			"    --gravity <gravity>        icon positioning gravity (one of NW, NE, SW, SE)\n"
			"    --grow-gravity <gravity>   tray`s grow gravity (one of N, S, W, E, NW, NE, SW, SE)\n"
			"    -i, --icon-size <n>        set basic icon size to <n>\n"
			"    -h, --help                 show this message\n"
			"    -n, --no-deco              hide window manager`s decorations\n"
			"    --no-border                hide tray window`s border\n"
			"    --no-title                 hide tray window`s title\n"
			"    --permanent                reacquire tray selection after loss\n"
			"    -p, --parent-bg            use parent for background\n"
			"    --skip-taskbar             hide tray`s window from the taskbar\n"
			"    --sticky                   make tray`s window sticky across multiple desktops/pages\n"
			"    -t, --transparent          enable root transparency\n"
			"    -v, --vertical             use vertical layout of icons (horizontal layout"
			"                               is used by default)\n"
			"    --xsync                    operate on X server synchronously (SLOW)\n"
			"\n",
		  progname);
}

int parse_cmdline(int argc, char **argv, int preliminary) 
{
	char *progname = (char *) basename(argv[0]);
	char *tail;
	int i, ok;

	while (--argc > 0) {
		argv++;
		DBG(6, "cmdline param: %s\n", argv[0]);
		ok = 0;
		for (i = 0; i < NPARAMS; i++) {
			if ((params[i].prm_shrt != NULL && !strcmp(argv[0], params[i].prm_shrt)) ||
				(params[i].prm_long != NULL && !strcmp(argv[0], params[i].prm_long))) 
			{
				ok = 1;
				if (preliminary <= params[i].preliminary) {
					switch (params[i].type) {
					case CLP_FLAG:
						*((int *)params[i].target) = 1;
						break;
					case CLP_INT:
					case CLP_STRING:
						argc--;
						if (argc == 0) {
							ERR("%s is missing an argument\n", argv[0]);
							return FAILURE;						}
						argv++;

						if (params[i].type == CLP_INT) {
							parse_int(argv[0], (int *)params[i].target);
						} else {
							*((char **)params[i].target) = strdup(argv[0]);
						}
						
						break;
					}
				} else {
					if (params[i].type != CLP_FLAG) {
						argc--;
						argv++;
					}
				}
				break;
			}
		}
		
		if (!ok) {
			ERR("%s is not a valid cmd line option\n", argv[0]);
			usage(progname);
			exit(-1);
		}
	}

	if (settings.need_help) {
		usage(progname);
		exit(0);
	}
	
	return SUCCESS;
}

void interpret_settings()
{
	int geom_flags; 
	XWindowAttributes root_wa;
	
	settings.gravity = parse_gravity(settings.gravity_str, GRAV_N | GRAV_E);
	if (!(settings.gravity & GRAV_V)) {
		settings.gravity |= GRAV_N;
	}
	if (!(settings.gravity & GRAV_H)) {
		settings.gravity |= GRAV_E;
	}
	settings.grow_gravity = parse_gravity(settings.grow_gravity_str, GRAV_N | GRAV_E);

	if (settings.transparent && settings.parent_bg) settings.parent_bg = 0;

	switch (settings.gravity) {
	case (GRAV_S|GRAV_E):
		settings.gravity_x = SouthEastGravity;
		break;
	case (GRAV_S|GRAV_W):
		settings.gravity_x = SouthWestGravity;
		break;
	case (GRAV_N|GRAV_E):
		settings.gravity_x = NorthEastGravity;
		break;
	case (GRAV_N|GRAV_W):
	default:
		settings.gravity_x = NorthWestGravity;
	}

	trap_errors();
	
	XParseColor(tray_data.dpy, XDefaultColormap(tray_data.dpy, DefaultScreen(tray_data.dpy)),
			settings.bg_color_str, &settings.bg_color);
	XAllocColor(tray_data.dpy, XDefaultColormap(tray_data.dpy, DefaultScreen(tray_data.dpy)), 
			&settings.bg_color);

	if (untrap_errors(tray_data.dpy)) {
		ERR("bad color specification \"%s\"", settings.bg_color_str);
		settings.bg_color.pixel = 0x777777;
	}

	trap_errors();

	XGetWindowAttributes(tray_data.dpy, RootWindow(tray_data.dpy, DefaultScreen(tray_data.dpy)), &root_wa);

	DBG(4, "the root window geometry %dx%d\n", root_wa.width, root_wa.height);

	if (untrap_errors(tray_data.dpy)) {
		DIE("could not get root window dimensions. WEIRD...\n");
	}

	geom_flags = XParseGeometry(settings.geometry_str, 
					&tray_data.xsh.x, &tray_data.xsh.y,
					&tray_data.xsh.width, &tray_data.xsh.height);

	DBG(4, "the geometry from XParseGeometry: %dx%d at (%d,%d)\n", 
			tray_data.xsh.width, tray_data.xsh.height,
			tray_data.xsh.x, tray_data.xsh.y);
	
	if (geom_flags & XNegative) {
		tray_data.xsh.x = root_wa.width + tray_data.xsh.x - tray_data.xsh.width;
	}

	if (geom_flags & YNegative) {		
		tray_data.xsh.y = root_wa.height + tray_data.xsh.y - tray_data.xsh.height;
	}

	if (!settings.max_tray_width)
		settings.max_tray_width = root_wa.width;
	else
		if (settings.max_tray_width < settings.icon_size)
			settings.max_tray_width = settings.icon_size;
	
	if (!settings.max_tray_height)
		settings.max_tray_height = root_wa.height;
	else
		if (settings.max_tray_height < settings.icon_size)
			settings.max_tray_height = settings.icon_size;

}

/************ .stalonetrayrc *****/

/**************************************************************************************
 * <line> ::= [<whitespaces>] [(<arg> <whitespaces>)* <arg>] [<whitespaces>] <comment>
 * <arg> ::= "<arbitrary-text>"|<text-without-spaces-and-#>
 * <comment> ::= # <arbitrary-text>
 **************************************************************************************/

#define SKIP_SPACES(p) { for (; *p != 0 && isspace((int) *p); p++); }

int get_args(char *line, int *argc, char ***argv)
{
	int q_flag = 0;
	char *arg_start, *q_pos;
	
	*argc = 0;
	*argv = NULL;
	
	/* 1. Strip leading spaces */
	SKIP_SPACES(line);
	if (0 == *line) { /* meaningless line */
		return True;
	}
	arg_start = line;

	/* 2. Strip comments */
	for (; 0 != *line; line++) {
		q_flag = ('"' == *line) ? !q_flag : q_flag; /* XXX ? */
		if ('#' == *line && !q_flag) {
			*line = 0;
			break;
		}
	}
	if (q_flag) { /* disbalance of quotes */
		ERR("Disbalance of quotes\n");
		return False;
	}
	if (arg_start == line) { /* meaningless line */
		return True;
	}
	line--;
	
	/* 3. Strip trailing spaces */	
	for (; line != arg_start && isspace((int) *line); line--);
	if (arg_start == line) { /* meaningless line */
		return True;
	}
	*(line + 1) = 0; /* this _is_ really ok since isspace(0) != 0 */
	line = arg_start;
	
	/* 4. Extract arguments */
	do {
		(*argc)++;
		if (NULL == (*argv = realloc(*argv, *argc * sizeof(char *)))) {
			free(*argv);
			ERR("Out of memory\n");
			return False;
		}
		
		if (*arg_start == '"') { /* 4.1. argument is quoted: find matching quote */
			arg_start++;
			(*argv)[*argc - 1] = arg_start;
			if (NULL == (q_pos = strchr(arg_start, '"'))) {
				DBG(0, "internal err: no matching quote found\n");
				free(*argv);
				return False;
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

}

#define READ_BUF_SZ	512

void parse_rc()
{
	wordexp_t exp_result;
	FILE *cfg;

	char buf[READ_BUF_SZ + 1];
	int lnum = 0;

	int argc;
	char **argv;

	DBG(8, "unparsed cfg file \"%s\"\n", settings.config_fname);

	/* 1. Expand everything in settings.config_fname, to get 
	 *    normal file name */
	switch(wordexp(settings.config_fname, &exp_result, 0)) {
	case 0: /* success */
		break;
	case WRDE_NOSPACE: /* just in case */
		wordfree(&exp_result);
	default: /* some other error */
		ERR("Could parse config file name\n");
		return;
	}

	settings.config_fname = strdup(exp_result.we_wordv[0]);
	wordfree(&exp_result);

	DBG(4, "using config file \"%s\"\n", settings.config_fname);

	/* 2. Open file */
	cfg = fopen(settings.config_fname, "r");
	if (cfg == NULL) {
		ERR("could not open %s (%s)\n", settings.config_fname, strerror(errno));
		return;
	}

#define REQ_ARGS(num) {\
	if (argc - 1 != num) { \
		free(argv); \
		ERR("%s:%d:%s invalid number of args (%d required)\n", settings.config_fname, lnum, argv[0], num); \
		break; \
	} \
}
	
	/* 3. Read the file */
	buf[READ_BUF_SZ] = 0;
	while (!feof(cfg)) {
		lnum++;

		if (fgets(buf, READ_BUF_SZ, cfg) == NULL) {
			if (errno) ERR("read error (%s)\n", strerror(errno));
			break;
		}

		get_args(buf, &argc, &argv);

		if (!argc) continue; /* empty/comment-only line */

		if (!strcasecmp(argv[0], "background")) {
			REQ_ARGS(1);
			settings.bg_color_str = strdup(argv[1]);
#ifdef DEBUG
		} else if (!strcasecmp(argv[0], "dbg_level")) {
			REQ_ARGS(1);
			parse_int(argv[1], &settings.dbg_level);
#endif
		} else if (!strcasecmp(argv[0], "display")) {
			REQ_ARGS(1);
			settings.display_str = strdup(argv[1]);
		} else if (!strcasecmp(argv[0], "geometry")) {
			REQ_ARGS(1);
			settings.geometry_str = strdup(argv[1]);
		} else if (!strcasecmp(argv[0], "gravity")) {
			REQ_ARGS(1);
			settings.gravity_str = strdup(argv[1]);
		} else if (!strcasecmp(argv[0], "grow_gravity")) {
			REQ_ARGS(1);
			settings.grow_gravity_str = strdup(argv[1]);
		} else if (!strcasecmp(argv[0], "icon_size")) {
			REQ_ARGS(1);
			parse_int(argv[1], &settings.icon_size);
			settings.icon_size = settings.icon_size < FALLBACK_SIZE ? FALLBACK_SIZE : settings.icon_size;
		} else if (!strcasecmp(argv[0], "max_height")) {
			REQ_ARGS(1);
			parse_int(argv[1], &settings.max_tray_height);
		} else if (!strcasecmp(argv[0], "max_width")) {
			REQ_ARGS(1);
			parse_int(argv[1], &settings.max_tray_width);
		} else if (!strcasecmp(argv[0], "no_deco")) {
			REQ_ARGS(1);
			settings.hide_wnd_deco = parse_bool(argv[1], settings.hide_wnd_deco);
		} else if (!strcasecmp(argv[0], "no_border")) {
			REQ_ARGS(1);
			settings.hide_wnd_border = parse_bool(argv[1], settings.hide_wnd_border);
		} else if (!strcasecmp(argv[0], "no_title")) {
			REQ_ARGS(1);
			settings.hide_wnd_title = parse_bool(argv[1], settings.hide_wnd_title);
		} else if (!strcasecmp(argv[0], "parent_bg")) {
			REQ_ARGS(1);
			settings.parent_bg = parse_bool(argv[1], settings.parent_bg);
		} else if (!strcasecmp(argv[0], "permanent")) {
			REQ_ARGS(1);
			settings.parent_bg = parse_bool(argv[1], settings.permanent);
		} else if (!strcasecmp(argv[0], "skip_taskbar")) {
			REQ_ARGS(1);
			settings.skip_taskbar = parse_bool(argv[1], settings.skip_taskbar);
		} else if (!strcasecmp(argv[0], "sticky")) {
			REQ_ARGS(1);
			settings.sticky = parse_bool(argv[1], settings.sticky);
		} else if (!strcasecmp(argv[0], "transparent")) {
			REQ_ARGS(1);
			settings.transparent = parse_bool(argv[1], settings.transparent);
		} else if (!strcasecmp(argv[0], "vertical")) {
			REQ_ARGS(1);
			settings.vertical = parse_bool(argv[1], settings.vertical);
		} else if (!strcasecmp(argv[0], "window_layer")) {
			REQ_ARGS(1);
			settings.wnd_layer = parse_wnd_layer(argv[1], settings.wnd_type);
		} else if (!strcasecmp(argv[0], "window_type")) {
			REQ_ARGS(1);
			settings.wnd_type = parse_wnd_type(argv[1], settings.wnd_type);
		} else if (!strcasecmp(argv[0], "xsync")) {
			REQ_ARGS(1);
			settings.xsync = parse_bool(argv[1], settings.xsync);
		} else {
			ERR("%s:%d: unknown directive \"%s\"\n", settings.config_fname, lnum, argv[0]);
		}

		free(argv);
	}
}

/************** "main" ***********/

int read_settings(int argc, char **argv) {
	parse_cmdline(argc, argv, 1);
	parse_rc();
	parse_cmdline(argc, argv, 0);

	DBG(4, "--flags--\n");
	DBG(4, "parent_bg = %d\n", settings.parent_bg);
	DBG(4, "hide_wnd_deco = %d\n", settings.hide_wnd_deco);
	DBG(4, "min_space_policy = %d\n", settings.min_space_policy);
	DBG(4, "full_pmt_search = %d\n", settings.full_pmt_search);
	DBG(4, "minimal_movement = %d\n", settings.minimal_movement);
	DBG(4, "need_help = %d\n", settings.need_help);
	DBG(4, "xsync = %d\n", settings.xsync);
	DBG(4, "vertical = %d\n", settings.vertical);
	DBG(4, "--strings--\n");
	DBG(4, "display_str = \"%s\"\n", settings.display_str);
	DBG(4, "bg_color_str = \"%s\"\n", settings.bg_color_str);
	DBG(4, "geometry_str = \"%s\"\n", settings.geometry_str);
	DBG(4, "gravity_str = \"%s\"\n", settings.gravity_str);
	DBG(4, "grow_gravity_str = \"%s\"\n", settings.grow_gravity_str);
	DBG(4, "config_fname = \"%s\"\n", settings.config_fname);
	DBG(4, "--values--\n");
	DBG(4, "icon_size = %d\n", settings.icon_size);
	DBG(4, "grow_gravity = 0x%x\n", settings.grow_gravity);
	DBG(4, "gravity = 0x%x\n", settings.gravity);
	DBG(4, "max_tray_width = %d\n", settings.max_tray_width);
	DBG(4, "max_tray_height = %d\n", settings.max_tray_height);
#ifdef DEBUG
	DBG(4, "dbg_level = %d\n", settings.dbg_level);
#endif

	return SUCCESS;
}
