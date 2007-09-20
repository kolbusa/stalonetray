/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* embed.c
* Fri, 03 Sep 2004 20:38:55 +0700
* -------------------------------
* embedding cycle implementation
* -------------------------------*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "embed.h"

#include "debug.h"
#include "icons.h"
#include "xembed.h"
#include "kde_tray.h"
#include "settings.h"

extern TrayIcon *head;

#ifdef DEBUG

void check_window_presence(Display *dpy, Window w)
{
	Window root;
	int x, y, code;
	unsigned int width, height, bw, depth;

	trap_errors();

	XGetGeometry(dpy, w, &root, &x, &y, &width, &height, &bw, &depth);
	XSync(dpy, False);

	if ((code = untrap_errors())) {
		DBG(0, "0x%x is gone(%d)?\n", w, code);
	}
}

#else

void check_window_presence(Display *dpy, Window w)
{
}

#endif

int embed_icon(Display *dpy, Window tray, Window w)
{
	TrayIcon *tmp;
	XWindowAttributes xwa;
	int s, x, y;
	unsigned long xembed_flags;

	if ((tmp = find_icon(w)) == NULL) {	
		DBG(2, "not mine\n");
		return 0;
	}

	/* mid-parent is created anyway */
	trap_errors();
	tmp->l.p = XCreateSimpleWindow(dpy, tray, tmp->l.x, tmp->l.y, 
	                                          tmp->l.gw * settings.icon_size, tmp->l.gh * settings.icon_size,
											  0, 0, 0);

	XSelectInput(dpy, tmp->l.p, SubstructureNotifyMask | StructureNotifyMask );

	XSetWindowBackgroundPixmap(dpy, tmp->l.p, ParentRelative);

	if (s = untrap_errors()) {
		DBG(0, "failed to create mid-parent (%d)\n", s);
		goto bailout;
	}

	DBG(2, "created mid-parent 0x%x\n", tmp->l.p);

	trap_errors();
	XGetWindowAttributes(dpy, tmp->w, &xwa);
	if (s = untrap_errors()) {
		DBG(0, "failed to get w attributes (%d)\n", s);
		goto bailout;
	}

	x = (tmp->l.gw * settings.icon_size - xwa.width) / 2;
	y = (tmp->l.gh * settings.icon_size - xwa.height) / 2;

	switch (tmp->cmode) {
		case CM_KDE:
		case CM_FDO: /* XXX: maybe this should go to it's own function ? */
			trap_errors();
			XReparentWindow(dpy, tmp->w, tmp->l.p, x, y);
			if (s = untrap_errors()) {
				DBG(0, "failed to reparent w (%d)\n", s);
				goto bailout;
			}

			trap_errors();
			
			xembed_flags = xembed_get_info(dpy, tmp->w, NULL);

			if (xembed_flags & XEMBED_MAPPED)
				XMapRaised(dpy, tmp->w);

			s = xembed_embeded_notify(dpy, tray, tmp->w);
			s = xembed_window_activate(dpy, tmp->w);
			s = xembed_focus_in(dpy, tmp->w, XEMBED_FOCUS_CURRENT);
			
			if (s = untrap_errors()) {
				DBG(0, "failed to embed w (%d)\n", s);
			}
			
			break;
		default:
			break;
	}

	trap_errors();
	XMapRaised(dpy, tmp->l.p);
	if (s = untrap_errors()) {
		DBG(0, "failed to map mid-parent (%d)\n", s);
		goto bailout;
	}

	DBG(3, "success\n");

	return 1;
	
bailout:
	unembed_icon(dpy, w);
	return 0;
}

int delete_parent(Display *dpy, TrayIcon *ti)
{
	Window r, p, *c = NULL;
	int nc;

	if (ti->l.p == 0)
		return 1;

	XQueryTree(dpy, ti->w, &r, &p, &c, &nc);

	if (c != NULL)
		XFree(&c);

	if (p == ti->l.p) {
		trap_errors();
		XReparentWindow(dpy, ti->w, DefaultRootWindow(dpy), ti->l.x, ti->l.y);
		if (nc = untrap_errors()) {
			DBG(0, "reparenting w to root failed (%d)\n", nc);
		}
	}

	trap_errors();
	XDestroyWindow(dpy, ti->l.p);
	if (nc = untrap_errors()) {
		DBG(0, "destroying of mid-parent failed (%d)\n", nc);
	}
	
	return (p == ti->l.p);
}

int unembed_icon(Display *dpy, Window w)
{
	TrayIcon *tmp;
	XWindowAttributes xwa;
	int s;

	if ((tmp = find_icon(w)) == NULL) {	
		DBG(2, "not mine\n");
		return 0;
	}

	switch (tmp->cmode) {
		case CM_KDE:
		case CM_FDO:
			DBG(1, "using FDO protocol\n");
			
			if (tmp->embeded) {
				trap_errors();
				XUnmapWindow(dpy, tmp->w);
				if (s = untrap_errors()) {
					DBG(0, "unmapping failed (%d)\n", s);
				}
			}

			delete_parent(dpy, tmp);

			break;

		default:
			fprintf(stderr, "Error: protocol not supported\n");
			return 0;
	}
	
	return 1;
}

