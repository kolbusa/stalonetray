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

/* Gravity constants */
/* East gravity */
#define GRAV_E	(1L << 0)
/* West gravity */
#define GRAV_W	(1L << 1)
/* South gravity */
#define GRAV_S	(1L << 2)
/* North gravity */
#define GRAV_N	(1L << 3)
/* Shortcut for horisontal gravity */
#define GRAV_H	(GRAV_W | GRAV_E)
/* Shortcut for vertical gravity */
#define GRAV_V	(GRAV_S | GRAV_N)

/* Add icon to layout */
int layout_add(struct TrayIcon *ti);

/* Remove icon from layout */
int layout_remove(struct TrayIcon *ti);

/* Relayout the icon which has changed its size */
int layout_handle_icon_resize(struct TrayIcon *ti);

/* Get current layout dimensions */
void layout_get_size(unsigned int *width, unsigned int *height);

/* Translate grid coordinates into window coordinates */
int grid2window(struct TrayIcon *ti);

/* Return next icon in tab chain */
struct TrayIcon *layout_next(struct TrayIcon *current);

/* Return previous icon in tab chain */
struct TrayIcon *layout_prev(struct TrayIcon *current);

#endif
