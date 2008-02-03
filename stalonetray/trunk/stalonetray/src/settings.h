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

/* Default name of configuration file */
#define STALONETRAY_RC	".stalonetrayrc"

struct Settings {
	/* Flags */
	int parent_bg;			/* Enable pseudo-transparency using parents' background*/
	int deco_flags;			/* Decoration flags */
	int transparent;		/* Enable root-transparency */
	int skip_taskbar;		/* Remove tray from  wm`s taskbar */
	int sticky;				/* Make tray sticky across desktops/pages */
	int xsync;				/* Operate on X server syncronously */
	int pixmap_bg;			/* Is pixmap used for background */
	int min_space_policy;	/* Use placement that cause minimal grow */
	int full_pmt_search; 	/* Use non-first-match search algorithm */
	int vertical;			/* Use vertical icon layout */
	int shrink_back_mode;	/* Keep tray's window size minimal */
	int start_withdrawn;	/* Start in withdrawn mode */
	int respect_icon_hints; /* Try to respect icon size hints */
	int ignore_icon_resize; /* Ignore icons attempts to resize its window */
	
	int need_help;			/* Print usage and exit */

	/* Strings */
	char *display_str;		/* Name of the display */
	char *bg_color_str;		/* Background color name */
	char *geometry_str;		/* Geometry spec */
	char *config_fname;		/* Path to the configuration file */
	char *wnd_type;			/* Window type */
	char *wnd_layer;		/* Window layed */
	char *window_role;		/* Window role */
	char *bg_pmap_path;		/* Background pixmap path */
	char *tint_color_str;	/* Color used for tinting */

	/* Values */
	int icon_size;			/* Icon size */
	int grow_gravity;		/* Icon gravity (interpretation of icon_gravity_str) */
	int icon_gravity;		/* Grow gravity (interpretation of grow_gravity_str) */
	int win_gravity;		/* Tray window gravity (computed using grow gravity) */
	int bit_gravity;		/* Tray window bit gravity (computed using icon_gravity) */
	int geom_gravity;		/* Tray window gravity when mapping the window (computed using geometry_str) */
	int max_tray_width;		/* Maximal tray width */
	int max_tray_height;	/* Maximal tray height */
	int max_layout_width;	/* Maximal layout width */
	int max_layout_height;	/* Maximal layout height */
	int fuzzy_edges;		/* Level of edges fuzziness (0 = disabled) */
	int tint_level;			/* Tinting level (0 = disabled) */

	int orig_width;			/* Original tray width */ 
	int orig_height;		/* Original tray height */

	XColor tint_color;		/* Color used for tinting */

#ifdef DELAY_EMBEDDING_CONFIRMATION
	int confirmation_delay;
#endif

	XColor bg_color;		/* Tray background color */
#ifdef DEBUG	
	int dbg_level;			/* Debug level */
#endif
};

extern struct Settings settings;

/* Read settings from cmd line and configuration file */
int read_settings(int argc, char **argv); 
/* Interpret all settings that either need an 
 * open display or are interpreted from other
 * settings */
void interpret_settings();

#endif
