/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* kde_tray.c
* Sun, 19 Sep 2004 12:31:10 +0700
* -------------------------------
* kde tray related routines
* -------------------------------*/

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "debug.h"
#include "xembed.h"

Window check_kde_tray_icons(Display *dpy, Window w)
{
	/* for XQueryWindow */
	Window root, parent, *children = NULL;
	int nchildren;
	Atom xa_kde_net_wm_system_tray_window_for;
	/* for XGetWindowProperty */
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	Window tray_icon_for;

	int s, i;
	Window r;

	trap_errors();
	xa_kde_net_wm_system_tray_window_for = XInternAtom(dpy, "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR", False);

	XGetWindowProperty(dpy, w, xa_kde_net_wm_system_tray_window_for, 0L, sizeof(Window),
			False, XA_WINDOW, &actual_type, &actual_format, &nitems, &bytes_after,
			(unsigned char **) &tray_icon_for);

	if (s = untrap_errors()) {
		DBG(0, "0x%x gone?\n", w);
		return None;
	}

	if (actual_type == XA_WINDOW && actual_format != None && tray_icon_for != None) {
		DBG(0, "0x%x is KDE tray icon\n", w);
		return w;
	}

	trap_errors();
	XQueryTree(dpy, w, &root, &parent, &children, &nchildren);
	if (s = untrap_errors()) {
		DBG(2, "0x%x gone?\n", w);
		if (children != NULL) {
			XFree(children);
		}
		return None;
	}

	for (i = 0; i < nchildren; i++) {
		r = check_kde_tray_icons(dpy, children[i]);

		if (r != None) {
			XFree(children);
			return r;
		}
	}

	return None;
}
