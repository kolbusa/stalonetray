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

#include "common.h"
#include "tray.h"
#include "debug.h"
#include "icons.h"
#include "kde_tray.h"
#include "settings.h"

#include "xembed.h"
#include "xutils.h"

#define CALC_INNER_POS(x_, y_, ti_) do { \
	x_ = (ti_->l.icn_rect.w - ti_->l.wnd_sz.x) / 2; \
	y_ = (ti_->l.icn_rect.h - ti_->l.wnd_sz.y) / 2; \
} while (0);

int embed_window(struct TrayIcon *ti)
{
	int x, y;
	XSetWindowAttributes xswa;

	/* 1. Calc. position of mid-parent */
	CALC_INNER_POS(x, y, ti);
	DBG(6, ("position of the icon inside the parent: (%d, %d)\n", x, y));
	
	/* 2. Create mid-parent */
	trap_errors();
	ti->l.p = XCreateSimpleWindow(tray_data.dpy, tray_data.tray, 
				ti->l.icn_rect.x + x, ti->l.icn_rect.y + y, 
				ti->l.wnd_sz.x, ti->l.wnd_sz.y, 0, 0, 0);

	xswa.win_gravity = settings.bit_gravity;
	XChangeWindowAttributes(tray_data.dpy, ti->l.p, CWWinGravity, &xswa);

	XSelectInput(tray_data.dpy, ti->l.p, SubstructureNotifyMask | StructureNotifyMask);
#ifdef DEBUG
	XSelectInput(tray_data.dpy, ti->w, StructureNotifyMask | ExposureMask | VisibilityChangeMask);
#endif
	XSetWindowBackgroundPixmap(tray_data.dpy, ti->l.p, ParentRelative);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("failed to create a mid-parent (%d)\n", trapped_error_code));
		goto bailout;
	}

	DBG(6, ("created the mid-parent 0x%x\n", ti->l.p));
	
	/* 3. Embed window into mid-parent */
	switch (ti->cmode) {
		case CM_KDE:
		case CM_FDO:
			trap_errors();

			XReparentWindow(tray_data.dpy, ti->w, ti->l.p, 0, 0);

			if (!wait_for_event_serial(ti->w, ReparentNotify, &ti->reparent_serial))
				goto bailout;
			
			/* XXX: Hate to say that, but WTF? */
			DBG(9, ("serial: %u\n", ti->reparent_serial));

			if (untrap_errors(tray_data.dpy)) {
				DBG(0, ("failed to reparent icon`s window (%d)\n", trapped_error_code));
				goto bailout;
			}

			trap_errors();

			xembed_embed(ti);

#if 0			
			xembed_flags = xembed_get_info(tray_data.dpy, ti->w, &xembed_version);
			
			DBG(3, ("xembed flags: 0x%x, version: 0x%x\n", xembed_flags, xembed_version));
#endif
#if 0 /* always map the window */
			if (xembed_flags & XEMBED_MAPPED) {
				trap_errors();
#endif
				XMapRaised(tray_data.dpy, ti->w);
				if (!wait_for_event(ti->w, MapNotify))
					goto bailout;
#if 0
			}
#endif
#if 0
			xembed_embedded_notify(tray_data.dpy, tray_data.tray, ti->w);
#endif	
#if 0
			xembed_window_activate(tray_data.dpy, ti->w);
			xembed_focus_in(tray_data.dpy, ti->w, XEMBED_FOCUS_CURRENT);
#endif			
			if (untrap_errors(tray_data.dpy)) {
				DBG(0, ("failed to embed icon`s window (%d)\n", trapped_error_code));
			}
			
			break;
		default:
			break;
	}

	/* 4. Show mid-parent */
	trap_errors();
	
	XMapRaised(tray_data.dpy, ti->l.p);
	wait_for_event(ti->l.p, MapNotify);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("failed to map midparent 0x%x (%d)\n", ti->l.p, trapped_error_code));
		goto bailout;
	}
	
	DBG(4, ("success\n"));
	ti->invalid = False;
/*	ti->embedded = True; */
	return SUCCESS;
	
bailout:
	untrap_errors(tray_data.dpy);
	unembed_window(ti);
	DBG(4, ("failed\n"));
	return FAILURE;
}

int unembed_window(struct TrayIcon *ti)
{
	DBG(6, ("unembedding 0x%x\n", ti->w));

	if (!ti->embedded) return SUCCESS;

	switch (ti->cmode) {
		case CM_KDE:
		case CM_FDO:
			if (ti->embedded) {
				trap_errors();

				xembed_unembed(ti);

				XUnmapWindow(tray_data.dpy, ti->w);
				XReparentWindow(tray_data.dpy, ti->w, DefaultRootWindow(tray_data.dpy),
						ti->l.icn_rect.x, ti->l.icn_rect.y);
				XMapWindow(tray_data.dpy, ti->w);
				
				if (untrap_errors(tray_data.dpy)) {
					DBG(0, ("failed to move 0x%x out of the tray (%d)\n", 
						ti->w, trapped_error_code));
				}
			}

			if (ti->l.p != None) {
				trap_errors();

				XDestroyWindow(tray_data.dpy, ti->l.p);
				
				if (untrap_errors(tray_data.dpy)) {
					DBG(0, ("failed to destroy the mid-parent (%d)\n", trapped_error_code));
				}
			}
			
			break;

		default:
			DBG(0, ("Error: the protocol %d is not supported (should not happen)\n", ti->cmode));
			return FAILURE;
	}
	
	return SUCCESS;
}

int update_window_pos(struct TrayIcon *ti)
{
	int x, y;

	if (!ti->l.update_pos && !ti->l.resized && ti->embedded)
		return NO_MATCH;

	DBG(8, ("Updating the position of 0x%x\n", ti->w));

	CALC_INNER_POS(x, y, ti);
	
	ti->l.resized = False;
	ti->l.update_pos = False;

	trap_errors();

	XMoveResizeWindow(tray_data.dpy, ti->l.p, 
				ti->l.icn_rect.x + x, ti->l.icn_rect.y + y, 
				ti->l.wnd_sz.x, ti->l.wnd_sz.y);

	XMoveWindow(tray_data.dpy, ti->w, 0, 0);

	XClearWindow(tray_data.dpy, ti->l.p);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("failed to update the position of 0x%x (%d)\n", ti->w, trapped_error_code));
		ti->invalid = True;
	}

	return NO_MATCH;
}

int update_positions()
{
	forall_icons(&update_window_pos);
	DBG(4, ("icon position were updated\n"));
	return SUCCESS;
}
