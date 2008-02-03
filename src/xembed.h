/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* icons.h
* Tue, 24 Aug 2004 12:05:38 +0700
* -------------------------------
* XEMBED protocol implementation
* -------------------------------*/

#ifndef _XEMBED_H_
#define _XEMBED_H_

#include <X11/X.h>

#include "icons.h"

/* Data structure for all XEMBED-related data for the tray */
struct XEMBEDData {
	struct 	TrayIcon *current;		/* Pointer to the currently focused icon */
	struct 	XEMBEDAccel *accels;	/* List of registered XEMBED accelerators */
	int 	window_has_focus;		/* Flag: does tray's window have focus */
	int		focus_requested;		/* Flag: if there is not completed focus request */
	Window 	focus_proxy;			/* Window ID of XEMBED focus proxy */
	long 	timestamp;				/* Timestamp of current processed message */
	Atom 	xa_xembed_info;			/* Atom: XEMBED_INFO */
	Atom 	xa_xembed;				/* Atom: XEMBED */
};

/* Initialize XEMBED data structures */
void xembed_init();

/* Event handling routine for XEMBED support */
void xembed_handle_event(XEvent ev);

/* Check if icon ti supports XEMBED */
int xembed_check_support(struct TrayIcon *ti);

/* Send XEMBED embedding acknowledgement to icon ti */
int xembed_embed(struct TrayIcon *ti);

/* Same as above for unembedding */
int xembed_unembed(struct TrayIcon *ti);

/* Get XEMBED mapped state from XEMBED info */
int xembed_get_mapped_state(struct TrayIcon *ti);

/* Set XEMBED mapped state in XEMBED info */
int xembed_set_mapped_state(struct TrayIcon *ti, int state);

#endif
