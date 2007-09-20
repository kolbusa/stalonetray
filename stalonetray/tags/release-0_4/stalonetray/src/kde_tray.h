/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* kde_tray.h
* Sun, 19 Sep 2004 12:28:59 +0700
* -------------------------------
* kde tray related routines
* -------------------------------*/

#ifndef _KDE_TRAY_H_
#define _KDE_TRAY_H_

Window check_kde_tray_icons(Display *dpy, Window w); 
void collect_kde_tray_icons(Display *dpy, Window **icons, unsigned long *len);

#endif
