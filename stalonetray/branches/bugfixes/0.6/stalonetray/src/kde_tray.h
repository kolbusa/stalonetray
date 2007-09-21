/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* kde_tray.h
* Sun, 19 Sep 2004 12:28:59 +0700
* -------------------------------
* kde tray related routines
* -------------------------------*/

#ifndef _KDE_TRAY_H_
#define _KDE_TRAY_H_

int update_old_kde_icons(Display *dpy);
void kde_tray_support_init(Display *dpy);
int is_old_kde_tray_icon(Window w);
void unmark_old_kde_tray_icon(Window w);
int is_kde_tray_icon(Display *dpy, Window w);

#endif
