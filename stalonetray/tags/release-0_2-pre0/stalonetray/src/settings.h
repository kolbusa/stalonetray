/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * settings.h
 * Sun, 12 Sep 2004 18:06:08 +0700
 * -------------------------------
 * settings parser\container
 * -------------------------------*/

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include "layout.h"

#define STALONETRAY_RC	"$HOME/.stalonetrayrc"

typedef struct {
	int icon_size;
	
	int grow_gravity;
	int gravity;
	
	int border_width;
	int max_tray_width;
	int max_tray_height;
	int vertical;

	int parent_bg;
	int hide_wnd_deco;

	char *dpy_name;
	char *bg_color_name;
	char *geometry;

	char *gravity_str;
	char *grow_gravity_str;

	int need_help;

	int xsync;
#ifdef DEBUG	
	int dbg_level;
#endif 

	ForceEntry	*f_head;

} Settings;

extern Settings settings;

int read_settings(int argc, char **argv);

#endif
