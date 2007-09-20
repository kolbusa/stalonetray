/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * settings.c
 * Sun, 12 Sep 2004 18:55:53 +0700
 * -------------------------------
 * settings parser\container
 * -------------------------------*/

#include <X11/Xlib.h>
#include <X11/X.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "settings.h"
#include "debug.h"
#include "layout.h"
#include "list.h"

Settings settings;

/************ CLI **************/

typedef struct {
	char	*name;
	int		type;
	
	char	**cval;
	int		*val;
	
	char	*cdef;
	int 	def;
	
	char	*desc;
} CmdLineParam;

typedef struct {
	int		id;
	char	*value;
} CmdLineToken;
#ifdef DEBUG
	#define NPARAMS			22
#else
	#define NPARAMS			21
#endif

#define DEF_G			"100x100+100+100"

#define CLP_FLG	0
#define CLP_VAL	1

CmdLineParam params[NPARAMS] = {
	{"-d",				CLP_VAL,	&settings.dpy_name,			NULL,						NULL,	0,	" displayname\t\t: Xserver to contact"},
	{"-display",		CLP_VAL,	&settings.dpy_name,			NULL,						NULL,	0,	" displayname\t: same"},
#ifdef DEBUG
	{"--dbg-level",		CLP_VAL,	NULL,						&settings.dbg_level,		NULL,	0,  " level\t: control amount of debug info"},
#endif
	{"--background",	CLP_VAL,	&settings.bg_color_name,	NULL,						NULL,	0,	" colour\t: background colour"},
	{"-bg",				CLP_VAL,	&settings.bg_color_name,	NULL,						NULL,	0,	" colour\t\t: same"},
	{"--max-width",		CLP_VAL,	NULL,						&settings.max_tray_width,	NULL,	0,	" width\t: tray's maximal width"},
	{"--max-height",	CLP_VAL,	NULL,						&settings.max_tray_height,	NULL,	0,	" height\t: tray's maximal height"},
	{"--gravity",		CLP_VAL,	&settings.gravity_str,		NULL,						NULL,	0,	" gravity\t: placement gravity (NE, NW, SE, SW)"},
	{"--grow-gravity",	CLP_VAL,	&settings.grow_gravity_str,	NULL,						NULL,	0,	" gravity\t: grow directions (N, S, E, W, NE, NW, SE, SW)"},
	{"-i",				CLP_VAL,	NULL,						&settings.icon_size,		NULL,	24,	" size\t\t\t: base icon size"},
	{"--icon-size",		CLP_VAL,	NULL,						&settings.icon_size,		NULL,	24,	" size\t: same"},
	{"-g",				CLP_VAL,	&settings.geometry,			NULL,						DEF_G,	0,	" geometry\t\t: tray's geometry"},
	{"--geometry",		CLP_VAL,	&settings.geometry,			NULL,						DEF_G,	0,	" geometry\t: same"},
	{"-h",				CLP_FLG,	NULL,						&settings.need_help,		NULL,	0,	"\t\t\t: show this help message"},
	{"--help",			CLP_FLG,	NULL,						&settings.need_help,		NULL,	0,	"\t\t\t: same"},
	{"-n",				CLP_FLG,	NULL,						&settings.hide_wnd_deco,	NULL,	0,  "\t\t\t: dont show window decoration"},
	{"--no-deco",		CLP_FLG,	NULL,						&settings.hide_wnd_deco,	NULL,	0,	"\t\t: same"},
	{"-p",				CLP_FLG,	NULL,						&settings.parent_bg,		NULL,	0,	"\t\t\t: make window parent draw its background"},
	{"--parent-bg",		CLP_FLG,	NULL,						&settings.parent_bg,		NULL,	0,	"\t\t: same"},
	{"-v",				CLP_FLG,	NULL,						&settings.vertical,			NULL,	0,	"\t\t\t: switch to vertical layout"},
	{"--vertical",		CLP_FLG,	NULL,						&settings.vertical,			NULL,	0,	"\t\t: same"},
	{"--xsync",			CLP_FLG,	NULL,						&settings.xsync,			NULL,	0,  "\t\t\t: operate on X server synchronously (SLOOOOW)"}
};

void usage(char *progname) {
	int i;

	printf("\nUsage: %s [args]\n\nPossible args are:\n", progname);
	
	for (i = 0; i < NPARAMS; i++)
		printf("\t%s%s\n", params[i].name, params[i].desc);
}

void init_params()
{
	int i;

	for (i = 0; i < NPARAMS; i++) {
		if (params[i].val != NULL) {
			*params[i].val = params[i].def;
		}
		if (params[i].cval != NULL) {
			*params[i].cval = params[i].cdef;
		}
	}
}

int parse_int(char *str, int def)
{
	int r;
	char *tail;
	
	r = strtol(str, &tail, 0);

	if (tail == str) {
		return def;
	} else {
		return r;
	}
}

int parse_cmdline(int argc, char **argv) 
{
	char *progname = (char *) basename(argv[0]);
	char *tail;
	int i;

	while (--argc > 0) {
		argv++;
		DBG(3, "param: %s\n", argv[0]);
		for (i = 0; i < NPARAMS; i++) {
			if (!strcmp(argv[0], params[i].name)) {
				switch (params[i].type) {
					case CLP_VAL:
						argv++;
						argc--;
				
						if (argc == 0) {
							printf("Error: %s needs an argument\n", params[i].name);
							usage(progname);
							return 0;
						}

						if (params[i].val != NULL) {
							errno = 0;
							*params[i].val = strtol(argv[0], &tail, 0);
							if (errno || tail == argv[0]) {
								printf("Error: %s : failed to parse an argument\n", params[i].name);
								usage(progname);
								return 0;
							}
						}

						if (params[i].cval != NULL) {
							*params[i].cval = argv[0];
						}
						
						break;
					case CLP_FLG:
						*params[i].val = 1;
						
						break;
					default:
						break;
				}

				i = NPARAMS;
			}
		}
	}
}

char *get_dpy_name(int argc, char **argv)
{
	argc--;
	argv++;
	while (argc > 0) {
		if (!strcmp(argv[0], "-d") || !strcmp(argv[0], "--display") || !strcmp(argv[0], "--display")) {
			argc--;
			argv++;
			return argv[0];
		}
		argc--;
		argv++;
	}

	return NULL;
}

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
}

void interpret_params()
{
	settings.gravity = parse_gravity(settings.gravity_str, GRAV_N | GRAV_W);
	if (!(settings.gravity & GRAV_V)) {
		settings.gravity |= GRAV_N;
	}
	if (!(settings.gravity & GRAV_H)) {
		settings.gravity |= GRAV_E;
	}
	settings.grow_gravity = parse_gravity(settings.grow_gravity_str, GRAV_N | GRAV_W);
}

/************ X resources *******/

void parse_resources(Display *dpy)
{
	DBG(0, "NOT_IMPLEMENTED\n");
}

/************ .stalonetrayrc *****/

#define READ_BUF_SZ	256

#define SKIP_SPACES(p)	{ for (; *p != 0 && isspace((int) *p); p++); }
#define FIND_SPACE(p)	{int _q = 0; for (; *p != 0; p++) {if (*p == '"') {_q = !_q;} else if (isspace((int) *p) && !_q && *(p-1) != '\\') {*p = 0; p++; break;}}}

void unescape(char *str)
{
	char *p = str, *p1 = str;
	
	DBG(4, "before: %s\n", str);

	if (*p == '"') {
		p++;
	}

	while (*p) {
		if (*p == '\\') {
			p++;
			switch (*p) {
			case 'n':
				*p1++ = '\n';
				break;
			case 't':
				*p1++ = '\t';
				break;
			case '\\':
				*p1++ = '\\';
				break;
			default:
				*p1++ = *p++;
				continue;
			}
			p++;
		} else {
			*p1++ = *p++;
		}
	}
	
	*p1 = 0;

	if (*(--p1) == '"') {
		*p1 = 0;
	}
	
	DBG(4, "after: %s\n", str);

}

#define ARG_MISSED(l, n)	fprintf(stderr, "rc %d: %s: argument missing\n", l, n);

int parse_bool(char *str, int def)
{
	if (strcasecmp(str, "yes") || strcasecmp(str, "on") || strcasecmp(str, "true") || strcasecmp(str, "1")) {
		return 1;
	}
	
	if (strcasecmp(str, "no") || strcasecmp(str, "off") || strcasecmp(str, "false") || strcasecmp(str, "0")) {
		return 0;
	}

	return def;
}

void add_force_entry(char *g, char *p)
{
	ForceEntry *tmp;
	int x, y, w, h;

	tmp = (ForceEntry *) malloc(sizeof(ForceEntry));

	XParseGeometry(g, &x, &y, &w, &h);

	tmp->width = w;
	tmp->height = h;

	tmp->pattern = strdup(p);

	DBG(2, "%s -> %dx%d\n", p, w, h);

	LIST_ADD_ITEM(settings.f_head, tmp);
}


void parse_rc(Display *dpy)
{

	char *name, *arg1 = NULL, *arg2 = NULL, buf[READ_BUF_SZ], *tmp, *line;
	int lnum = 0, quotes = 0;
	char fname[1024];

	fname[1023] = 0;

	snprintf(fname, sizeof(fname) - 1, "%s/.stalonetrayrc", getenv("HOME"));

	FILE *config = fopen(fname, "r");

	if (config == NULL) {
		fprintf(stderr, "Could not open %s (%s).\n", fname, strerror(errno));
		return;
	}

	while (!feof(config)) {
		lnum++;

		if (fgets(buf, READ_BUF_SZ, config) == NULL) {
			if (errno) {
				DBG(0, "read error (%s).\n", strerror(errno));
			}
			break;
		}

		line = buf;
		
		tmp = line;

		for (; *tmp != 0; tmp++) {
			if (*tmp == '"') {
				quotes = !quotes;
			} else if (*tmp == '#') {
				if (*(tmp-1) != '\\' && !quotes) {
					*tmp = 0;
					break;
				}
			}
		}

		SKIP_SPACES(line);

		if (*line == 0) {
			continue;
		}

		name = line;

		FIND_SPACE(line);
		SKIP_SPACES(line);

		unescape(name);

		arg1 = line;

		FIND_SPACE(line);
		SKIP_SPACES(line);

		unescape(arg1);
		arg2 = line;

		FIND_SPACE(line);

		unescape(arg2);

		DBG(4, "name=%s, arg1=%s, arg2=%s\n", name, arg1, arg2);

		if (*arg1 == 0) {
			fprintf(stderr, "Warning: no arguments on line %d\n", lnum);
		}

		if (!strcasecmp(name, "vertical")) {
			settings.vertical = parse_bool(arg1, settings.vertical);
			continue;
		}

		if (!strcasecmp(name, "gravity")) {
			settings.gravity = parse_gravity(arg1, settings.gravity);
			continue;
		}

		if (!strcasecmp(name, "grow_gravity")) {
			settings.grow_gravity = parse_gravity(arg1, settings.grow_gravity);
			continue;
		}

		if (!strcasecmp(name, "background")) {
			settings.bg_color_name = strdup(arg1);
			continue;
		}

		if (!strcasecmp(name, "display")) {
			settings.dpy_name = strdup(arg1);
			continue;
		}

		if (!strcasecmp(name, "parent_bg")) {
			settings.parent_bg = parse_bool(arg1, settings.parent_bg);
			continue;
		}

		if (!strcasecmp(name, "no_deco")) {
			settings.hide_wnd_deco = parse_bool(arg1, settings.hide_wnd_deco);
			continue;
		}

		if (!strcasecmp(name, "geometry")) {
			settings.geometry = strdup(arg1);
			continue;
		}

		if (!strcasecmp(name, "max_width")) {
			settings.max_tray_width = parse_int(arg1, settings.max_tray_width);
			continue;
		}

		if (!strcasecmp(name, "max_height")) {
			settings.max_tray_height = parse_int(arg1, settings.max_tray_height);
			continue;
		}

		if (!strcasecmp(name, "dbg_level")) {
			settings.dbg_level = parse_int(arg1, settings.dbg_level);
			continue;
		}

		if (!strcasecmp(name, "xsync")) {
			settings.xsync = parse_bool(arg1, settings.xsync);
			continue;
		}

		if (!strcasecmp(name, "force")) {
			if (*arg2 == 0) {
				fprintf(stderr, "Error: not enought arguments for \"force\" on line %d\n", lnum);
			} else {
				add_force_entry(arg1, arg2);
			}
			continue;
		}

		fprintf(stderr, "Warning: nonsence on line %d\n", lnum);
	}

	fclose(config);
}

/************** "main" ***********/

int read_settings(int argc, char **argv) {
	Display *dpy;
	
	dpy = XOpenDisplay(get_dpy_name(argc, argv));

	init_params();

	parse_resources(dpy);
	parse_rc(dpy);

	if (dpy != NULL) {
		XCloseDisplay(dpy);
	}

	if (!parse_cmdline(argc, argv))
		return 0;

	if (settings.need_help) {
		usage((char *)basename(argv[0]));
		return 0;
	}

	interpret_params();

	return 1;
}
