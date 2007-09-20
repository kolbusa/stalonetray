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

#include "tray.h"
#include "debug.h"
#include "icons.h"
#include "xembed.h"
#include "kde_tray.h"
#include "settings.h"

#define CALC_INNER_POS(x_, y_, ti_) { \
	x_ = (ti_->l.icn_rect.w - ti_->l.wnd_sz.x) / 2; \
	y_ = (ti_->l.icn_rect.h - ti_->l.wnd_sz.y) / 2; \
}
	

int embed_window(struct TrayIcon *ti)
{
	int x, y;
	unsigned long xembed_flags;
	XWindowAttributes xwa;

	/* 1. Calc. position of mid-parent */
	CALC_INNER_POS(x, y, ti);
	DBG(2, "pos inside the parent: (%d, %d)\n", x, y);
	
	/* 2. Create mid-parent */
	trap_errors();
	ti->l.p = XCreateSimpleWindow(tray_data.dpy, tray_data.tray, 
				ti->l.icn_rect.x + x, ti->l.icn_rect.y + y, 
				ti->l.wnd_sz.x, ti->l.wnd_sz.y, 0, 0, 0);

	XSelectInput(tray_data.dpy, ti->l.p, SubstructureNotifyMask | StructureNotifyMask );
	
	XSetWindowBackgroundPixmap(tray_data.dpy, ti->l.p, ParentRelative);

	XMapRaised(tray_data.dpy, ti->l.p);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, "failed to create mid-parent (%d)\n", trapped_error_code);
		goto bailout;
	}

	DBG(2, "created mid-parent 0x%x\n", ti->l.p);
	
	/* 3. Embed window into mid-parent */
	switch (ti->cmode) {
		case CM_KDE:
		case CM_FDO:
			trap_errors();
			XReparentWindow(tray_data.dpy, ti->w, ti->l.p, 0, 0);
			if (untrap_errors(tray_data.dpy)) {
				DBG(0, "failed to reparent w (%d)\n", trapped_error_code);
				goto bailout;
			}

			trap_errors();
			
			xembed_flags = xembed_get_info(tray_data.dpy, ti->w, NULL);

			if (xembed_flags & XEMBED_MAPPED)
				XMapRaised(tray_data.dpy, ti->w);

			xembed_embeded_notify(tray_data.dpy, tray_data.tray, ti->w);
			xembed_window_activate(tray_data.dpy, ti->w);
			xembed_focus_in(tray_data.dpy, ti->w, XEMBED_FOCUS_CURRENT);
			
			if (untrap_errors(tray_data.dpy)) {
				DBG(0, "failed to embed w (%d)\n", trapped_error_code);
			}
			
			break;
		default:
			break;
	}

	/* 4. Show mid-parent */
	trap_errors();
	
	XMapRaised(tray_data.dpy, ti->w);
	
	if (untrap_errors(tray_data.dpy)) {
		DBG(0, "failed to map 0x%x (%d)\n", ti->w, trapped_error_code);
		goto bailout;
	}

	usleep(1000);
#if 0
	trap_errors();

	XGetWindowAttributes(tray_data.dpy, ti->w, &xwa);	

	DBG(4, "current 0x%x pos: (%d, %d)\n", ti->w, xwa.x, xwa.y);

	if (xwa.x != 0 || xwa.y != 0) {
		DBG(2, "trying to insist on placement\n");
		XMoveWindow(tray_data.dpy, ti->w, 0, 0);
	}
	
	if (untrap_errors(tray_data.dpy)) {
		DBG(0, "something wicked happened to 0x%x (%d)\n", ti->w, trapped_error_code);
	}
#endif
	
	DBG(3, "success\n");

	return 1;
	
bailout:
	unembed_window(ti);
	return 0;
}

int delete_parent(struct TrayIcon *ti)
{
	Window r, p, *c = NULL;
	int nc;

	if (ti->l.p == 0)
		return 1;

	XQueryTree(tray_data.dpy, ti->w, &r, &p, &c, &nc);

	if (c != NULL)
		XFree(&c);

	if (p == ti->l.p) {
		trap_errors();
		XReparentWindow(tray_data.dpy, ti->w, DefaultRootWindow(tray_data.dpy),
				ti->l.icn_rect.x, ti->l.icn_rect.y);
		if (untrap_errors(tray_data.dpy)) {
			DBG(0, "reparenting w to root failed (%d)\n", trapped_error_code);
		}
	}

	trap_errors();
	XDestroyWindow(tray_data.dpy, ti->l.p);
	if (untrap_errors(tray_data.dpy)) {
		DBG(0, "destroying of mid-parent failed (%d)\n", trapped_error_code);
	}
	
	return (p == ti->l.p);
}

int unembed_window(struct TrayIcon *ti)
{
	switch (ti->cmode) {
		case CM_KDE:
		case CM_FDO:
			if (ti->embeded) {
				trap_errors();
				XUnmapWindow(tray_data.dpy, ti->w);
				if (untrap_errors(tray_data.dpy)) {
					DBG(0, "unmapping failed (%d)\n", trapped_error_code);
				}
			}

			delete_parent(ti);

			break;

		default:
			fprintf(stderr, "Error: protocol not supported (should not happen)\n");
			return 0;
	}
	
	return 1;
}

int update_icon_pos(struct TrayIcon *ti)
{
	int x, y;
	
	if (!ti->l.update_pos)
		return 0;

	CALC_INNER_POS(x, y, ti);

	trap_errors();

	XMoveWindow(tray_data.dpy, ti->l.p, ti->l.icn_rect.x + x, ti->l.icn_rect.y + y);
	
	if (untrap_errors(tray_data.dpy)) {
		DBG(0, "failed to update position of 0x%x (%d)\n", ti->w, trapped_error_code);
	} else {
		ti->l.update_pos = 0;
	}

	return 0;
}



int update_positions()
{
	forall_icons(&update_icon_pos);
}
