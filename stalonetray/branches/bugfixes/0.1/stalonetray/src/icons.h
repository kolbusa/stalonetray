/*
 * Manipulations with reparented windows --- tray icons
 */

#ifndef _ICONS_H_
#define _ICONS_H_

#define GRAV_E	(1 << 0)
#define GRAV_W	0
#define GRAV_S	(1 << 1)
#define GRAV_N	0

#define GRAV_V	(1 << 1)
#define GRAV_H	(1 << 0)

int add_icon(Display *dpy, Window tray, Window w);
int rem_icon(Display *dpy, Window tray, Window w, int reparent);

int new_icon_baloon(Window w, int len);
int update_icon_ballon(Window w, char *data);

void clean_icons(Display *dpy);

void set_reparented(Window w);

int relayout_icons(Display *dpy, Window tray, int width, int height, int force);

#endif
