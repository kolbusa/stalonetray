/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* kde_tray.h
* Sun, 19 Sep 2004 12:28:59 +0700
* -------------------------------
* KDE tray related routines
* -------------------------------*/

#ifndef _KDE_TRAY_H_
#define _KDE_TRAY_H_

/* Init support for KDE tray icons. */
void kde_tray_init(Display *dpy);

/* Check if WM supports KDE tray icons */
int kde_tray_update_fallback_mode(Display *dpy);

/* Update the list of "old" KDE icons. Icon is considered "old"
 * if it was present before the tray was started. */
int  kde_tray_update_old_icons(Display *dpy);

/* Check if the window  w is an "old" KDE tray icon */
int  kde_tray_is_old_icon(Window w);

/* Remove the window w from the list of "old" KDE tray icons */
void kde_tray_old_icons_remove(Window w);

/* Check if the window w is a KDE tray icon */
int  kde_tray_check_for_icon(Display *dpy, Window w);

/* Find KDE tray icon in subwindows of w */
Window kde_tray_find_icon(Display *dpy, Window w);

#endif
