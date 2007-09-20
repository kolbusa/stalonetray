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

#define STALONETRAY_RC	"$HOME/.stalonetrayrc"

struct Settings {
	/* flags */
	int parent_bg;			/* pseudo-transparency */
	int hide_wnd_deco;		/* hide wnd deco */
	int transparent;
	
	int min_space_policy;	/* use placement that cause minimal grow */
	int full_pmt_search; 	/* use non-first-match search algo */
	int minimal_movement;	/* minimaly move icons on resize */
	
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

	ForceEntry	*f_head;

};

extern struct Settings settings;

int read_settings(int argc, char **argv);
void interpret_settings();

#endif
