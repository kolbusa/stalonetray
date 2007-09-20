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
#include "xutils.h"

void collect_kde_tray_icons(Display *dpy, Window **icons, unsigned long *len)
{
	Atom atom, act_type;
	Window root_wnd;
	int act_fmt;
	unsigned long bytes_after, prop_len, wnd_list_len;
	Window *wnd_list = NULL;
	
	root_wnd = DefaultRootWindow(dpy);
	atom = XInternAtom(dpy, "_KDE_NET_SYSTEM_TRAY_WINDOWS", False);

	XGetWindowProperty(dpy, DefaultRootWindow(dpy), atom, 
		    0L, 0L, False, XA_WINDOW, &act_type, &act_fmt,
		&prop_len, &bytes_after, (unsigned char**)&wnd_list);

	prop_len = bytes_after / sizeof(Window);

	XGetWindowProperty(dpy, DefaultRootWindow(dpy), atom,
			0L, prop_len, False, XA_WINDOW, &act_type, &act_fmt,
			&wnd_list_len, &bytes_after, (unsigned char **)&wnd_list);

	*len = wnd_list_len;
	*icons = wnd_list;
}

Window check_kde_tray_icons(Display *dpy, Window w)
{
	/* for XQueryWindow */
	Window root, parent, *children = NULL;
	unsigned int nchildren;
	static Atom xa_kde_net_wm_system_tray_window_for = None;
	/* for XGetWindowProperty */
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	Window tray_icon_for;

	int i;
	Window r = None;

	trap_errors();
	if (xa_kde_net_wm_system_tray_window_for == None)
		xa_kde_net_wm_system_tray_window_for = XInternAtom(dpy, "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR", False);

	XGetWindowProperty(dpy, w, xa_kde_net_wm_system_tray_window_for, 0L, sizeof(Window),
			False, XA_WINDOW, &actual_type, &actual_format, &nitems, &bytes_after,
			(unsigned char **) &tray_icon_for);

	if (untrap_errors(dpy)) {
		DBG(1, ("0x%x seems to be gone\n", w));
		return None;
	}

	if (actual_type == XA_WINDOW && actual_format != None && tray_icon_for != None) {
		DBG(3, ("0x%x is the KDE tray icon\n", w));
		return w;
	}

	trap_errors();

	XQueryTree(dpy, w, &root, &parent, &children, &nchildren);

	if (untrap_errors(dpy)) {
		DBG(1, ("0x%x seems to be gone\n", w));
		goto bailout;
	}

	for (i = 0; i < nchildren; i++) 
		if ((r = check_kde_tray_icons(dpy, children[i])) != None) goto bailout;

	r = None;
bailout:
	if (children != NULL && nchildren > 0) XFree(children);
	return r;
}


