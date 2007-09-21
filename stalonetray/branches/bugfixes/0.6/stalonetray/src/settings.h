 /* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * settings.h
 * Sun, 12 Sep 2004 18:06:08 +0700
 * -------------------------------
 * settings parser\container
 * -------------------------------*/

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <X11/X.h>
#include <X11/Xlib.h>

#include "config.h"
#include "layout.h"

#define STALONETRAY_RC	".stalonetrayrc"

struct Settings {
	/* flags */
	int parent_bg;			/* flag: enable pseudo-transparency using parents' background*/
	int hide_wnd_deco;		/* flag: hide wnd deco */
	int hide_wnd_title;		/* flag: hide wnd title */
	int hide_wnd_border;	/* flag: hide wnd border */
	int transparent;		/* flag: enable root-transparency */
	int skip_taskbar;		/* flag: remove tray from  wm`s taskbar */
	int sticky;				/* flag: make tray sticky across desktops/pages */
	int xsync;				/* flag: operate on X server syncronously */
	int pixmap_bg;			/* flag: is pixmap used for background */
	int min_space_policy;	/* flag: use placement that cause minimal grow */
	int full_pmt_search; 	/* flag: use non-first-match search algorithm */
	int minimal_movement;	/* flag: minimally move icons on resize */
	int vertical;			/* flag: use vertical icon layout */
	int respect_icon_hints; /* flag: do respect icon window`s min_width/min_height size hints */
	int ignore_icon_resize; /* flag: force icon dimentions to be icon_size x icon_size */
	
	int need_help;			/* flag: print usage and exit */

	/* strings */
	char *display_str;		/* string: name of the display */
	char *bg_color_str;		/* string: background color name */
	char *geometry_str;		/* string: geometry spec */

	char *icon_gravity_str; /* string: icon gravity spec */
	char *grow_gravity_str; /* string: grow gravity spec */
	
	char *config_fname;		/* string: path to the configuration file */

	char *wnd_type;			/* string: window type */
	char *wnd_layer;		/* string: window layed */
	char *window_role;		/* string: window role */
	
	char *bg_pmap_path;		/* string: background pixmap path */

	/* values */
	int icon_size;			/* value: icon size */
	
	int grow_gravity;		/* value: icon gravity (interpretation of icon_gravity_str) */
	int icon_gravity;		/* value: grow gravity (interpretation of grow_gravity_str) */

	int win_gravity;		/* value: tray window gravity (computed using grow gravity) */
	int bit_gravity;		/* value: tray window bit gravity (computed using icon_gravity) */
	int geom_gravity;		/* value: tray window gravity when mapping the window (computed using geometry_str) */
	
	int max_tray_width;		/* value: maximal tray width */
	int max_tray_height;	/* value: maximal tray height */

	XColor bg_color;		/* value: tray background color */

#ifdef DEBUG	
	int dbg_level;			/* value: debug level */
#endif
};


extern struct Settings settings;

/* reads the settings from cmd line and configuration file */
int read_settings(int argc, char **argv); 
/* interprets the settings which need an open display */
void interpret_settings();

#endif
