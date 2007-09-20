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

	if (ti->is_hidden) {
		trap_errors();
		XSelectInput(tray_data.dpy, ti->wid, PropertyChangeMask);
		if (untrap_errors(tray_data.dpy)) {
			ti->is_invalid = True;
			return FAILURE;
		}
		return SUCCESS;
	}

	/* 1. Calc. position of mid-parent */
	CALC_INNER_POS(x, y, ti);
	DBG(6, ("position of the icon inside the parent: (%d, %d)\n", x, y));
	
	/* 2. Create mid-parent */
	trap_errors();
	ti->mid_parent = XCreateSimpleWindow(tray_data.dpy, tray_data.tray, 
				ti->l.icn_rect.x + x, ti->l.icn_rect.y + y, 
				ti->l.wnd_sz.x, ti->l.wnd_sz.y, 0, 0, 0);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("failed to create a mid-parent (%d)\n", trapped_error_code));
		goto bailout;
	}
	
	xswa.win_gravity = settings.bit_gravity;

	trap_errors();
	XChangeWindowAttributes(tray_data.dpy, ti->mid_parent, CWWinGravity, &xswa);
	XSetWindowBackgroundPixmap(tray_data.dpy, ti->mid_parent, ParentRelative);
	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("failed to create a mid-parent (%d)\n", trapped_error_code));
		goto bailout;
	}
	
	DBG(6, ("created the mid-parent 0x%x\n", ti->mid_parent));
	
	/* 3. Embed window into mid-parent */
	switch (ti->cmode) {
		case CM_KDE:
		case CM_FDO:
			trap_errors();
			XReparentWindow(tray_data.dpy, ti->wid, ti->mid_parent, 0, 0);
			XMapRaised(tray_data.dpy, ti->wid);
			if (untrap_errors(tray_data.dpy)) {
				DBG(0, ("failed to embed icon`s window (%d)\n", trapped_error_code));
			}
			break;
		default:
			break;
	}

	/* 4. Show mid-parent */
	trap_errors();
	
	XMapRaised(tray_data.dpy, ti->mid_parent);
	
	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("failed to map midparent 0x%x (%d)\n", ti->mid_parent, trapped_error_code));
		goto bailout;
	}

	trap_errors();

	XSync(tray_data.dpy, False);

	send_client_msg32(tray_data.dpy, tray_data.tray, tray_data.tray, tray_data.xa_tray_opcode, 
			0, STALONE_TRAY_DOCK_CONFIRMED, ti->wid, 0, 0);

	XSelectInput(tray_data.dpy, ti->wid, StructureNotifyMask | PropertyChangeMask);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("failed to embed 0x%x\n", ti->wid));
		goto bailout;
	}

	DBG(4, ("success\n"));
	ti->is_invalid = False;
	ti->is_hidden = False;
	return SUCCESS;
	
bailout:
	unembed_window(ti);
	DBG(4, ("failed\n"));
	return FAILURE;
}

int unembed_window(struct TrayIcon *ti)
{
	DBG(6, ("unembedding 0x%x\n", ti->wid));

	if (!ti->is_embedded) return SUCCESS;

	switch (ti->cmode) {
		case CM_KDE:
		case CM_FDO:
			if (ti->is_embedded) {
				trap_errors();
				XSelectInput(tray_data.dpy, ti->wid, NoEventMask);
				XUnmapWindow(tray_data.dpy, ti->wid);
				XReparentWindow(tray_data.dpy, ti->wid, DefaultRootWindow(tray_data.dpy),
						ti->l.icn_rect.x, ti->l.icn_rect.y);
				XMapRaised(tray_data.dpy, ti->wid);
				
				if (untrap_errors(tray_data.dpy)) {
					DBG(0, ("failed to move 0x%x out of the tray (%d)\n", 
						ti->wid, trapped_error_code));
				}
			}

			if (ti->mid_parent != None) {
				trap_errors();

				XDestroyWindow(tray_data.dpy, ti->mid_parent);
				
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

int hide_window(struct TrayIcon *ti)
{
	trap_errors();
	XUnmapWindow(tray_data.dpy, ti->mid_parent);
	XSelectInput(tray_data.dpy, ti->wid, PropertyChangeMask);
	if (untrap_errors(tray_data.dpy)) {
		ti->is_invalid = True;
		return FAILURE;
	} else {
		ti->is_hidden = True;
		return SUCCESS;
	}
}

int show_window(struct TrayIcon *ti)
{
	unsigned int x, y;

	if (ti->mid_parent == None) {
		ti->is_hidden = False;
		return embed_window(ti);
	}

	trap_errors();
	/* 0. calculate new position for mid-parent */	
	CALC_INNER_POS(x, y, ti);
	/* 1. move mid-parent to new location */
	XMoveResizeWindow(tray_data.dpy, ti->mid_parent,
			ti->l.icn_rect.x + x, ti->l.icn_rect.y + y,
			ti->l.wnd_sz.x, ti->l.wnd_sz.y);
	/* 2. adjust icon position inside mid-parent */
	XMoveWindow(tray_data.dpy, ti->wid, 0, 0);
	/* 3. map icon ? */
	XMapRaised(tray_data.dpy, ti->wid);
	/* 4. map mid-parent */
	XMapWindow(tray_data.dpy, ti->mid_parent);
	XSelectInput(tray_data.dpy, ti->wid, StructureNotifyMask | PropertyChangeMask);
	if (untrap_errors(tray_data.dpy)) {
		ti->is_invalid = True;
		return FAILURE;
	} else {
		ti->is_hidden = False;
		return SUCCESS;
	}
}

static int update_forced = False;

int update_window_pos(struct TrayIcon *ti)
{
	int x, y;

	if (ti->is_hidden)
		return NO_MATCH;

	if (!update_forced && !ti->is_updated && !ti->is_resized && ti->is_embedded)
		return NO_MATCH;

	DBG(8, ("Updating the position of 0x%x\n", ti->wid));

	CALC_INNER_POS(x, y, ti);
	
	ti->is_resized = False;
	ti->is_updated = False;

	trap_errors();

	XMoveResizeWindow(tray_data.dpy, ti->mid_parent, 
				ti->l.icn_rect.x + x, ti->l.icn_rect.y + y, 
				ti->l.wnd_sz.x, ti->l.wnd_sz.y);

	XMoveWindow(tray_data.dpy, ti->wid, 0, 0);

	XClearWindow(tray_data.dpy, ti->mid_parent);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("failed to update the position of 0x%x (%d)\n", ti->wid, trapped_error_code));
		ti->is_invalid = True;
	}

	return NO_MATCH;
}

int update_positions()
{
	forall_icons(&update_window_pos);
	DBG(4, ("icon position were updated\n"));
	return SUCCESS;
}

int update_positions_forced()
{
	update_forced = True;
	update_positions();
	update_forced = False;
}
