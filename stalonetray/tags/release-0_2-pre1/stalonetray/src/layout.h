/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* layout.h
* Tue, 24 Aug 2004 12:19:27 +0700
* -------------------------------
* Icon layout implementation
* -------------------------------*/

#ifndef _LAYOUT_H_
#define _LAYOUT_H_

#include "icons.h"

typedef struct ForceEntry_ {
	struct ForceEntry_ *next, *prev;
	int width, height;
	char *pattern;
} ForceEntry;

#define GRAV_E	(1L << 0)
#define GRAV_W	(1L << 1)
#define GRAV_S	(1L << 2)
#define GRAV_N	(1L << 3)

#define GRAV_H	(GRAV_W | GRAV_E)
#define GRAV_V	(GRAV_S | GRAV_N)


/* layouts new/existing icon */
int layout_icon(struct TrayIcon *ti);
/* unlayout icon */
int unlayout_icon(struct TrayIcon *ti);
/* window resized */
int layout_handle_tray_resize();

#endif
