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

#include "config.h"
#include "layout.h"

#define STALONETRAY_RC	"~/.stalonetrayrc"

struct Settings {
	/* flags */
	int parent_bg;			/* pseudo-transparency */
	int hide_wnd_deco;		/* hide wnd deco */
	int hide_wnd_title;			/* hide wnd title */
	int hide_wnd_border;		/* hide wnd border */
	int transparent;		/* root-transparency */
	int skip_taskbar;		/* skip wm`s taskbar */
	int sticky;
	
	int min_space_policy;	/* use placement that cause minimal grow */
	int full_pmt_search; 	/* use non-first-match search algorithm */
	int minimal_movement;	/* minimally move icons on resize */
	
	int need_help;
	int xsync;
	int vertical;

	int permanent;

	/* strings */
	char *display_str;
	char *bg_color_str;
	char *geometry_str;

	char *gravity_str;
	char *grow_gravity_str;
	
	char *config_fname;

	char *wnd_type;
	char *wnd_layer;

	/* values */
	int icon_size;
	
	int grow_gravity;
	int gravity;
	int gravity_x;
	
	int max_tray_width;
	int max_tray_height;

#ifdef DEBUG	
	int dbg_level;
#endif

	XColor bg_color;

	char *window_role;
};

extern struct Settings settings;

int read_settings(int argc, char **argv);
void interpret_settings();

#endif
