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

#define GRAV_E	(1L << 0)
#define GRAV_W	(1L << 1)
#define GRAV_S	(1L << 2)
#define GRAV_N	(1L << 3)

#define GRAV_H	(GRAV_W | GRAV_E)
#define GRAV_V	(GRAV_S | GRAV_N)

#define FALLBACK_SIZE	24

/* layouts new/existing icon */
int layout_icon(struct TrayIcon *ti);
/* unlayout icon */
int unlayout_icon(struct TrayIcon *ti);
/* the name says it all */
int layout_handle_icon_resize(struct TrayIcon *ti);
/* get current layout boundaries */
void layout_get_size(unsigned int *width, unsigned int *height);

#endif
