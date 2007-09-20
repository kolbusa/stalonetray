/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* main.c
* Tue, 24 Aug 2004 12:19:48 +0700
* -------------------------------
* main is main
* -------------------------------*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include "debug.h"
#include "icons.h"
#include "layout.h"
#include "embed.h"
#include "kde_tray.h"
#include "settings.h"

#define PROGNAME PACKAGE

/* from System Tray Protocol Specification 
 * http://freedesktop.org/Standards/systemtray-spec/systemtray-spec-0.1.html */
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define	TRAY_SEL_ATOM "_NET_SYSTEM_TRAY_S"

/* just globals */
Display		*dpy;
Window		tray_wnd;

XSizeHints	xsh = {
	flags:	(PSize | PPosition),
	x:		100,
	y:		100,
	width:	10,
	height:	10
};

Atom 		xa_tray_selection;
Atom		xa_tray_opcode;
Atom		xa_tray_data;
Atom 		xa_wm_delete_window;
Atom		xa_wm_protocols;
Atom		xa_wm_take_focus;
Atom 		xa_xembed;

static int clean = 0;

extern TrayIcon *icons_head;

void clean_icons()
{
	DBG(1, "clean_icons()\n");
	while (icons_head != NULL) {
		unembed_icon(dpy, icons_head->w);
		del_icon(icons_head->w);
	}
}

void cleanup()
{
	DBG(0, "cleanup()\n");
	if (clean) {
		return;
	}
	clean_icons(dpy);
	clean = 1;
}

void term_sig_handler(int sig)
{
	cleanup();
	signal(sig, SIG_DFL);
	raise(sig);
}

int grow(int grow_request_x, int grow_request_y)
{
	XSizeHints xsh_old = xsh;
	int err;

	DBG(0, "grow(), request_x=%d, request_y=%d\n", grow_request_x, grow_request_y);
	

	if (!valid_placement(dpy, tray_wnd, grow_request_x, grow_request_y)) {
		return 0;
	}
	
	if (settings.grow_gravity & GRAV_H) {
		xsh.height += grow_request_y;
		if (settings.grow_gravity & GRAV_N) {
			xsh.y -= grow_request_y;
		}
	}

	if (settings.grow_gravity & GRAV_V) {
		xsh.width += grow_request_x;
		if (settings.grow_gravity & GRAV_E) {
			xsh.x -= grow_request_x;
		}
	}

	trap_errors();
	XMoveResizeWindow(dpy, tray_wnd, xsh.x, xsh.y, xsh.width, xsh.height);
	if (err = untrap_errors()) {
		DBG(0, "could not move window (%d)\n", err);
		xsh = xsh_old;
		return 0;
	}
	
	return 1;
}

void set_constraints(int w, int h)
{
	XSizeHints xsh;
	XWindowAttributes xwa;
	long ret;

	XGetWindowAttributes(dpy, tray_wnd, &xwa);
	
	xsh.min_width = w;
	xsh.min_height = h; /*
	xsh.max_width = settings.max_tray_width;
	xsh.max_height = settings.max_tray_height; */
	xsh.base_width = settings.icon_size;
	xsh.base_height = settings.icon_size;
	xsh.width_inc = settings.icon_size;
	xsh.height_inc = settings.icon_size;
	xsh.width = settings.max_tray_width > xwa.width ? xwa.width : settings.max_tray_width;
	xsh.height = settings.max_tray_height > xwa.height ? xwa.height : settings.max_tray_height;
	xsh.flags = (PMinSize | PMaxSize | PResizeInc | PBaseSize | PSize);

	trap_errors();
	XSetWMNormalHints(dpy, tray_wnd, &xsh);
	if (ret = untrap_errors()) {
		DBG(1, "set_constraints(%d, %d) failed (%d)\n", w, h, ret);
	} else {
		DBG(1, "set_constraints(%d, %d) succeded\n", w, h);
	}

	XFlush(dpy);

	xsh.flags = PAllHints;

	XGetWMNormalHints(dpy, tray_wnd, &xsh, &ret);

	DBG(4, "ret=%x, flags=%x, max_w=%d, max_h=%d, min_w=%d, min_h=%d\n",
			ret, xsh.flags, xsh.max_width, xsh.max_width, xsh.min_width, xsh.min_height);
}

/* general embedder */
void embed(Window w, int cmode)
{
	DBG(0, "embeddint 0x%x\n", w);
	if (add_icon(w, cmode)) {
		if (!layout_icon(dpy, tray_wnd, w)) {
			goto bail_out;
		}
		if (!embed_icon(dpy, tray_wnd, w)) {
			goto bail_out;
		}
	}
	return;

bail_out:
	del_icon(w);
}

/* event handlers */

void reparent_notify(XReparentEvent ev)
{
	TrayIcon *tmp;
	
	tmp = find_icon(ev.window);

	if (tmp == NULL) {
		return;
	}

	if (tmp->l.p == ev.parent) {
		tmp->embeded = 1;
		return;
	}

	if (!tmp->embeded) {
		return;
	}

	if (tmp->l.p != ev.parent) {
		unembed_icon(dpy, ev.window);
		del_icon(ev.window);
		layout_pack(dpy, tray_wnd);
	}
}

void map_notify(XMapEvent ev)
{
	Window kde_tray_icon;
	TrayIcon *tmp;

	tmp = find_icon(ev.window);

	if (tmp != NULL) {
		return;
	}

	kde_tray_icon = check_kde_tray_icons(dpy, ev.window);

	if (kde_tray_icon != None) {
		embed(kde_tray_icon, CM_KDE);
	}
}

void client_message(XClientMessageEvent ev)
{
	if (ev.message_type == xa_wm_protocols && /* wm */
		ev.data.l[0] == xa_wm_delete_window)
	{
		exit(0);
	}

	if (ev.message_type == xa_tray_opcode) {
		DBG(2, "_NET_SYSTEM_TRAY_OPCODE(%u)\n", ev.data.l[1]);
		switch (ev.data.l[1]) {
			case SYSTEM_TRAY_REQUEST_DOCK:
				embed(ev.data.l[2], CM_FDO);
				break;
			case SYSTEM_TRAY_BEGIN_MESSAGE:
			case SYSTEM_TRAY_CANCEL_MESSAGE:
			default:
				break;
		}
	}

	if (ev.message_type == xa_xembed) {
		DBG(2, "_XEMBED message\n");
	}
}

void destroy_notify(XDestroyWindowEvent ev)
{
	if (ev.window != tray_wnd) {
		if (unembed_icon(dpy, ev.window)) {
			del_icon(ev.window);
			layout_pack(dpy, tray_wnd);
		}
	}	
}

void configure_notify(XConfigureEvent ev)
{
	if (ev.window == tray_wnd) {
		DBG(4, "x = %i, y = %i, width = %u, height = %u\n",
			ev.x, ev.y, ev.width, ev.height);
		if (ev.send_event) {
			xsh.x = ev.x;
			xsh.y = ev.y;
		}
					
		if ((ev.window != tray_wnd) ||
			(xsh.width == ev.width && xsh.height == ev.height))
		{
			return;
		}
		xsh.width = ev.width;
		xsh.height = ev.height;
		layout_pack(dpy, tray_wnd);
	} else {
		/* TODO: the window can change it's SIZE !!! we must handle it somehow --- especially,
		* if we can't grow anymore :/ */	
		layout_update(dpy, tray_wnd);
	}
}

int main(int argc, char** argv)
{
	XClassHint	xch;
	XWMHints	xwmh = {
		flags:	(InputHint | StateHint ),
		input:	True,
		initial_state: NormalState,
		icon_pixmap: None,
		icon_window: None,
		icon_x: 0,
		icon_y: 0,
		icon_mask: None,
		window_group: 0	
	};
	
	char *wnd_name = PROGNAME;
	XTextProperty wm_name;

	XWindowAttributes xwa;
	XEvent		ev;
	XColor		xc_bg = {
		flags:	0,
		pad:	0,
		pixel:	0x777777,
		red:	0,
		green:	0,
		blue:	0
	};

	char		*tray_sel_atom_name;

	long		mwm_hints[5] = {2, 0, 0, 0, 0};
	Atom		mwm_wm_hints;

	if (!read_settings(argc, argv)) {
		return 0;
	}
	
	/* register cleanups */
	atexit(cleanup);
	signal(SIGKILL, term_sig_handler);
	signal(SIGTERM, term_sig_handler);
	signal(SIGINT, term_sig_handler);
	
	/* here comes main stuff */
	if ((dpy = XOpenDisplay(settings.dpy_name)) == NULL)
		DIE("Error: could not open display\n");

	XParseGeometry(settings.geometry, &xsh.x, &xsh.y, &xsh.width, &xsh.height);
	DBG(4, "geometry (%s): %dx%d+%d+%d\n", settings.geometry, xsh.width, xsh.height, xsh.x, xsh.y);
	xsh.width = (settings.max_tray_width && xsh.width > settings.max_tray_width) ? settings.max_tray_width : xsh.width;
	xsh.height = (settings.max_tray_width && xsh.height > settings.max_tray_height) ? settings.max_tray_height : xsh.height;

	if (settings.bg_color_name != NULL) {
		DBG(4, "parsing bg\n");
		XParseColor(dpy, XDefaultColormap(dpy, DefaultScreen(dpy)),
					settings.bg_color_name, &xc_bg);
		DBG(4, "xc_bg.pixel=%u, xc_bg.red=%i, xc_bg.green=%i, xc_bg.blue=%i, xc_bg.flags=%i\n",
						xc_bg.pixel, xc_bg.red, xc_bg.green, xc_bg.blue, xc_bg.flags);
		XAllocColor(dpy, XDefaultColormap(dpy, DefaultScreen(dpy)),
					&xc_bg);
		DBG(4, "xc_bg.pixel=%u, xc_bg.red=%i, xc_bg.green=%i, xc_bg.blue=%i, xc_bg.flags=%i\n",
						xc_bg.pixel, xc_bg.red, xc_bg.green, xc_bg.blue, xc_bg.flags);
	}
	
	if (settings.xsync) {
		XSynchronize(dpy, True);
	}

	if ((tray_sel_atom_name = (char *)malloc(strlen(TRAY_SEL_ATOM) + 2)) == NULL)
		DIE("Error: low mem\n");

	snprintf(tray_sel_atom_name, strlen(TRAY_SEL_ATOM) + 2,
		"%s%u", TRAY_SEL_ATOM, DefaultScreen(dpy));

	DBG(4, "tray_sel_atom_name=%s\n", tray_sel_atom_name);

	xa_tray_selection = XInternAtom(dpy, tray_sel_atom_name, False);

	free(tray_sel_atom_name);
	
	xa_tray_opcode = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	xa_tray_data = XInternAtom(dpy, "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);

	xa_wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	xa_wm_take_focus = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	xa_wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);

	xa_xembed = XInternAtom(dpy, "_XEMBED", False);

	xch.res_class = PROGNAME;
	xch.res_name = PROGNAME;

	tray_wnd = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
					xsh.x, xsh.y, xsh.width, xsh.height, 0, xc_bg.pixel, xc_bg.pixel);

	XmbTextListToTextProperty(dpy, &wnd_name, 1, XTextStyle, &wm_name);

	XSetWMProperties(dpy, tray_wnd, &wm_name, NULL, argv, argc, &xsh, &xwmh, &xch);

	mwm_wm_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
	mwm_hints[2] = (settings.hide_wnd_deco == 0);
	XChangeProperty(dpy, tray_wnd, mwm_wm_hints, mwm_wm_hints, 32, 0, 
			(unsigned char *) mwm_hints, sizeof(mwm_hints));	

	if (settings.parent_bg)
		XSetWindowBackgroundPixmap(dpy, tray_wnd, ParentRelative);

	XSetSelectionOwner(dpy, xa_tray_selection, tray_wnd, CurrentTime);

	if (XGetSelectionOwner(dpy, xa_tray_selection) != tray_wnd)
		DIE("Error: could not set selection owner.\nMay be another (greedy) tray running?\n");
	
	xclient_msg32(dpy, RootWindow(dpy, DefaultScreen(dpy)),
					XInternAtom(dpy, "MANAGER", False),
					CurrentTime, xa_tray_selection, tray_wnd, 0, 0);

	set_constraints(settings.icon_size, settings.icon_size);

	XSetWMProtocols(dpy, tray_wnd, &xa_wm_delete_window, 1);
	
	XSelectInput(dpy, tray_wnd, SubstructureNotifyMask | StructureNotifyMask | PropertyChangeMask | SubstructureRedirectMask );
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureNotifyMask);
	
	XMapRaised(dpy, tray_wnd);
	XMoveWindow(dpy, tray_wnd, xsh.x, xsh.y);

	XFlush(dpy);

	for(;;) {
		while (XPending(dpy)) {
			XNextEvent(dpy, &ev);
			switch (ev.type) {
			case MapNotify:
				DBG(1, "MapNotify(0x%x)\n", ev.xmap.window);
				map_notify(ev.xmap);
				break;
			case PropertyNotify: /* TODO (non-critical) */
				DBG(1, "PropertyNotify(0x%x)\n", ev.xproperty.window);
				break;
			case DestroyNotify:
				DBG(1, "DestroyNotyfy(0x%x)\n", ev.xdestroywindow.window);
				destroy_notify(ev.xdestroywindow);
				break;
			case ClientMessage:
				DBG(1, "ClientMessage(from 0x%x?)\n", ev.xclient.window);
				client_message(ev.xclient);
				break;
			case ConfigureNotify:
				DBG(1, "ConfigureNotify(0x%x)\n", ev.xconfigure.window);
				configure_notify(ev.xconfigure);
				break;
			case ReparentNotify:
				DBG(1, "ReparentNotify(0x%x to 0x%x)\n", ev.xreparent.window, ev.xreparent.parent);
				reparent_notify(ev.xreparent);
				break;
			default:
				break;
			}
		}
		usleep(50000L);
	}

	return 0;
}
