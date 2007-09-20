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

struct XEMBEDAccel {
	struct XEMBEDAccel *next, *prev;
	struct TrayIcon *owner;
	char overloaded;
	long id;
	long symb;
	long mods;
};

struct XEMBEDData {
	struct TrayIcon *current;
	struct XEMBEDAccel *accels;
	int window_has_focus;
	Window focus_proxy;
	long timestamp;
};

void xembed_init();
void xembed_handle_event(XEvent ev);
void xembed_embed(struct TrayIcon *ti);
void xembed_unembed(struct TrayIcon *ti);

#endif
