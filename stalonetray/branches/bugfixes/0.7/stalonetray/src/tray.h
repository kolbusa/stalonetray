/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * tray.h
 * Wed, 29 Sep 2004 23:10:02 +0700
 * -------------------------------
 * Common tray routines
 * -------------------------------*/

#ifndef _TRAY_H_
#define _TRAY_H_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "config.h"
#include "common.h"
#include "icons.h"
#include "xembed.h"

/* Tray opcode messages from System Tray Protocol Specification 
 * http:freedesktop.org/Standards/systemtray-spec/systemtray-spec-0.2.html */
#define SYSTEM_TRAY_REQUEST_DOCK    0
/* These two are unused */
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2
/* Custom message: request for status */
#define STALONE_TRAY_STATUS_REQUESTED 0xFFFE
/* Custom message: confirmation of embedding */
#define STALONE_TRAY_DOCK_CONFIRMED 0xFFFF
/* Name of tray selection atom */
#define	TRAY_SEL_ATOM "_NET_SYSTEM_TRAY_S"
/* Name of tray orientation atom*/
#define TRAY_ORIENTATION_ATOM "_NET_SYSTEM_TRAY_ORIENTATION" 
/* Values of tray orientation property */
#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0
#define _NET_SYSTEM_TRAY_ORIENTATION_VERT 1

/* Window decoration flags */
#define DECO_BORDER	(1 << 0)
#define DECO_TITLE (1 << 1)
#define DECO_NONE 0
#define DECO_ALL (DECO_BORDER | DECO_TITLE)

/* Structure to hold all tray data */
struct TrayData {
	/* General */
	Window tray;							/* ID of tray window */
	Display *dpy;							/* Display pointer */
	Display *async_dpy;						/* Display which is used to send self asynchronous messages */
	XSizeHints xsh;							/* Size & position of the tray window */
	XSizeHints root_wnd;					/* Size & position :) of the root window */
	Window old_selection_owner;				/* Old owner of tray selection */
	int is_active;							/* Is the tray active? */
	int is_reparented;						/* Was the tray reparented in smth like FvwmButtons ? */
	int kde_tray_old_mode;					/* Use legacy scheme to handle KDE icons via MapNotify */

	/* Atoms */
	Atom xa_tray_selection;					/* Atom: _NET_SYSTEM_TRAY_SELECTION_S<creen number> */
	Atom xa_tray_opcode;					/* Atom: _NET_SYSTEM_TRAY_MESSAGE_OPCODE */
	Atom xa_tray_data;						/* Atom: _NET_SYSTEM_TRAY_MESSAGE_DATA */
	Atom xa_wm_protocols;					/* Atom: WM_PROTOCOLS */
	Atom xa_wm_delete_window;				/* Atom: WM_DELETE_WINDOW */
	Atom xa_wm_take_focus;					/* Atom: WM_TAKE_FOCUS */
	Atom xa_kde_net_system_tray_windows;	/* Atom: _KDE_NET_SYSTEM_TRAY_WINDOWS */
	Atom xa_net_client_list;				/* Atom: _NET_CLIENT_LIST */

	/* Background pixmap */
	Atom xa_xrootpmap_id;					/* Atom: _XROOTPMAP_ID */
	Atom xa_xsetroot_id;					/* Atom: _XSETROOT_ID */
	Pixmap bg_pmap;							/* Pixmap for tray background */
	unsigned int bg_pmap_width;				/* Width of background pixmap */
	unsigned int bg_pmap_height;			/* Height of background pixmap */

	/* XEMBED data*/
	struct XEMBEDData xembed_data;			/* XEMBED data */
};
extern struct TrayData tray_data;

/* Initialize all tray data structures */
void tray_init();
/* Create tray window */
void tray_create_window(int argc, char **argv);
/* Acquire tray selection */
void tray_acquire_selection();
/* Show tray window */
void tray_show_window();
/* Refresh tray window */
void tray_refresh_window(int exposures);
/* Update tray background (and pixmap, if update_pixmap is true) */
int tray_update_bg(int update_pixmap);
/* Update tray window size and hints */
int tray_update_window_size();
/* Set tray window WM hints */
int tray_set_wm_hints();

#endif
