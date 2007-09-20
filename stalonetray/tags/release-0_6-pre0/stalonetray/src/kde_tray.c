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
#include "kde_tray.h"

Window *old_kde_icons;
unsigned long n_old_kde_icons;

void kde_tray_support_init(Display *dpy)
{
	unsigned long n_client_windows, i;
	Window *client_windows;
	Atom xa_net_client_list;

	if (!collect_kde_tray_icons(dpy, &old_kde_icons, &n_old_kde_icons)) 
		return;

	xa_net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);

	if (!get_root_window_prop(dpy, xa_net_client_list, &client_windows, &n_client_windows))
		return;

	for (i = 0; i < n_client_windows; i++)
		unmark_old_kde_tray_icon(client_windows[i]);


	for (i = 0; i < n_old_kde_icons; i++) {
		Window root, parent, *children;
		unsigned int nchildren;
		nchildren = 0; children = NULL;
		trap_errors();
		if (XQueryTree(dpy, old_kde_icons[i], &root, &parent, &children, &nchildren)) {
			if (root == parent) old_kde_icons[i] = None;
			if (nchildren > 0) XFree(children);
		}
		if (untrap_errors(dpy))
			old_kde_icons[i] = None;
	}

#ifdef DEBUG
	for (i = 0; i < n_old_kde_icons; i++)
		if (old_kde_icons[i] != None)
			DBG(8, ("0x%x is marked as old KDE icon\n", old_kde_icons[i]));
#endif
}

int is_old_kde_tray_icon(Window w)
{
	int i;
	for (i = 0; i < n_old_kde_icons; i++)
		if (old_kde_icons[i] == w)
			return True;
	return False;
}

void unmark_old_kde_tray_icon(Window w)
{
	int i;
	for (i = 0; i < n_old_kde_icons; i++)
		if (old_kde_icons[i] == w) {
			DBG(8, ("Unmarking 0x%x as old kde icon\n", w));
			old_kde_icons[i] = None;
		}
}

int collect_kde_tray_icons(Display *dpy, Window **icons, unsigned long *len)
{
	return get_root_window_prop(dpy, tray_data.xa_kde_net_system_tray_windows, icons, len);
}

int is_kde_tray_icon(Display *dpy, Window w)
{
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	static Atom xa_kde_net_wm_system_tray_window_for = None;
	Window tray_icon_for;

	trap_errors();
	if (xa_kde_net_wm_system_tray_window_for == None)
		xa_kde_net_wm_system_tray_window_for = XInternAtom(dpy, "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR", False);

	XGetWindowProperty(dpy, w, xa_kde_net_wm_system_tray_window_for, 0L, sizeof(Window),
			False, XA_WINDOW, &actual_type, &actual_format, &nitems, &bytes_after,
			(unsigned char **) &tray_icon_for);

	if (untrap_errors(dpy)) {
		DBG(1, ("0x%x seems to be gone\n", w));
		return False;
	}

	if (actual_type != XA_WINDOW || nitems != 1) 
		return False;

	return True;
}

Window check_kde_tray_icons(Display *dpy, Window w)
{
	/* for XQueryWindow */
	Window root, parent, *children = NULL;
	unsigned int nchildren;

	int i;
	Window r = None;

	if (is_kde_tray_icon(dpy, w))
		return True;

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

