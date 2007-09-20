/* ************************************
 * vim:tabstop=4:shiftwidth=4
 * tray.c
 * Tue, 07 Mar 2006 10:36:10 +0600
 * ************************************
 * tray functions
 * ************************************/

#include "config.h"

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <unistd.h>

#include "common.h"
#include "tray.h"
#include "settings.h"
#include "xutils.h"
#include "wmh.h"

#ifndef NO_NATIVE_KDE
#include "kde_tray.h"
#endif

#include "debug.h"

void tray_init_data()
{
	tray_data.tray = None;
	tray_data.reparented = 0;
	tray_data.parent = None;
	tray_data.dpy = NULL;
	tray_data.grow_issued = 0;
	tray_data.root_pmap = None;
	tray_data.xa_xrootpmap_id = None;
	tray_data.xa_xsetroot_id = None;
	tray_data.xsh.flags = (PSize | PPosition);
	tray_data.xsh.x = 100;
	tray_data.xsh.y = 100;
	tray_data.xsh.width = 10;
	tray_data.xsh.height = 10;
}

int tray_update_bg()
{
	static Pixmap bg_pixmap = None;
	GC gc;
	XGCValues gc_values;
	
	DBG(3, ("updating tray`s transparent background\n"));

	if (tray_data.root_pmap == None) {
		DBG(0, ("no root pixmap found\n"));
		return FAILURE;
	}

	if (bg_pixmap != None) XFreePixmap(tray_data.dpy, bg_pixmap);

	trap_errors();

	bg_pixmap = XCreatePixmap(tray_data.dpy, tray_data.tray,
			tray_data.xsh.width, tray_data.xsh.height, 
			DefaultDepth(tray_data.dpy, DefaultScreen(tray_data.dpy)));

	if (bg_pixmap == None) {
		DBG(0, ("could not create bg pixmap\n"));
		untrap_errors(tray_data.dpy);
		return FAILURE;
	}
	
	DBG(8, ("created pixmap %x:%dx%d\n", bg_pixmap, tray_data.xsh.width, tray_data.xsh.height));
	
	gc = XCreateGC(tray_data.dpy, bg_pixmap, 0, &gc_values);

	XCopyArea(tray_data.dpy, tray_data.root_pmap, bg_pixmap, gc, 
			tray_data.xsh.x, tray_data.xsh.y, 
			tray_data.xsh.width, tray_data.xsh.height, 0, 0);

	DBG(8, ("XCopyArea(0x%p, 0x%x, 0x%x, 0x%x, %d, %d, %d, %d, %d, %d)\n",
			tray_data.dpy, tray_data.root_pmap, bg_pixmap, gc, 
			tray_data.xsh.x, tray_data.xsh.y, 
			tray_data.xsh.width, tray_data.xsh.height, 0, 0));

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("could not update background (%d)\n", trapped_error_code));
		return FAILURE;
	}

	DBG(8, ("the image is put onto pixmap\n"));	
	
	XSetWindowBackgroundPixmap(tray_data.dpy, tray_data.tray, bg_pixmap);

	XClearWindow(tray_data.dpy, tray_data.tray);

	forall_icons(&icon_repaint);

	return SUCCESS;
}


int tray_set_constraints() 
{
	XSizeHints xsh;
	XWindowAttributes xwa;

	layout_get_size((unsigned int*)&xsh.min_width, (unsigned int*)&xsh.min_height);

	if (xsh.min_width == 0) xsh.min_width = settings.icon_size;
	if (xsh.min_height == 0) xsh.min_height = settings.icon_size;
	
	xsh.width_inc = settings.icon_size;
	xsh.height_inc = settings.icon_size;
	
	xsh.base_width = settings.icon_size;
	xsh.base_height = settings.icon_size;

	xsh.max_width = settings.max_tray_width;
	xsh.max_height = settings.max_tray_height;

	xsh.flags = PResizeInc|PBaseSize|PMinSize|PMaxSize; 

	trap_errors();

	XSetWMNormalHints(tray_data.dpy, tray_data.tray, &xsh);
	XSetNormalHints(tray_data.dpy, tray_data.tray, &xsh);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("could not set tray's minimal size(%d)\n", trapped_error_code));
		return FAILURE;
	}

	return SUCCESS;
}

int tray_update_size()
{
	XWindowAttributes xwa;
	int wait_cd;
	
	if (!tray_data.grow_issued)
		return SUCCESS;

	tray_data.grow_issued = 0;
	
	trap_errors();

/*	if (!tray_data.reparented) */
		XMoveResizeWindow(tray_data.dpy, tray_data.tray, tray_data.xsh.x, tray_data.xsh.y,
				tray_data.xsh.width, tray_data.xsh.height);
/*	else {
		XResizeWindow(tray_data.dpy, tray_data.tray, tray_data.xsh.width, tray_data.xsh.height);
	}
*/
	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("could not move/resize window (%d)\n", trapped_error_code));
		return FAILURE;
	}

	/* XXX we need to be sure, that the size has really changed.
	 * this is dirty but seems to be unavoidable :( */
	/* XXX WE NEED A CONSTANT */
	for(wait_cd = 5; wait_cd >= 0; wait_cd--) {
		XGetWindowAttributes(tray_data.dpy, tray_data.tray, &xwa);

		if (tray_data.xsh.width == xwa.width && tray_data.xsh.height == xwa.height) {
			DBG(3, ("ok, tray`s size updated\n"));
			return SUCCESS;
		}
		
		DBG(8, ("current geometry: %dx%d, waiting for %dx%d\n",
				xwa.width, xwa.height, tray_data.xsh.width, xwa.height));
		
		if (wait_cd > 0) usleep(50000L);
	}

	DBG(0, ("failed to resize the tray, falling to %dx%d size\n", xwa.width, xwa.height));
	tray_data.xsh.width = xwa.width;
	tray_data.xsh.height = xwa.height;
	return SUCCESS;
}

int tray_grow(struct Point dsz)
{
	XSizeHints xsh_old = tray_data.xsh;
	int ok;

	DBG(3, ("grow(), dx=%d, dy=%d, grow_gravity=%c\n", dsz.x, dsz.y, settings.grow_gravity));
	DBG(4, ("old geometry:%dx%d+%d+%d\n", tray_data.xsh.width, tray_data.xsh.height, tray_data.xsh.x, tray_data.xsh.y));
	
	if (!tray_grow_check(dsz)) {
		return FAILURE;
	}
	
	if (settings.grow_gravity & GRAV_V) {
		tray_data.xsh.height += dsz.y;
		if (settings.grow_gravity & GRAV_S) {
			/*&& !tray_data.reparented) {*/
			tray_data.xsh.y -= dsz.y;
		}
	}

	if (settings.grow_gravity & GRAV_H) {
		tray_data.xsh.width += dsz.x;
		if (settings.grow_gravity & GRAV_E) {
			/*&& !tray_data.reparented) {*/
			tray_data.xsh.x -= dsz.x;
		}
	}
	
	DBG(3, ("new geometry:%dx%d+%d+%d\n", tray_data.xsh.width, tray_data.xsh.height, tray_data.xsh.x, tray_data.xsh.y));

	tray_data.grow_issued = 1;
	
	if (!tray_update_size()) 
		tray_data.xsh = xsh_old;

	return SUCCESS;
}

void tray_create_window(int argc, char **argv)
{
	char *wnd_name = PROGNAME;
	XTextProperty wm_name;

	XSetWindowAttributes xswa;
	
	XClassHint	xch;
	XWMHints	xwmh;
	
	Atom		net_system_tray_orientation;
	CARD32 		orient;

	xwmh.flags = AllHints; /*(InputHint | StateHint),*/
	xwmh.input = True;
	xwmh.initial_state = NormalState;
	xwmh.icon_pixmap = None;
	xwmh.icon_window = None;
	xwmh.icon_x = 0;
	xwmh.icon_y = 0;
	xwmh.icon_mask = None;
	xwmh.window_group = 0;

	tray_data.xa_wm_delete_window = XInternAtom(tray_data.dpy, "WM_DELETE_WINDOW", False);
	tray_data.xa_wm_take_focus = XInternAtom(tray_data.dpy, "WM_TAKE_FOCUS", False);
	tray_data.xa_wm_protocols = XInternAtom(tray_data.dpy, "WM_PROTOCOLS", False);

	xch.res_class = PROGNAME;
	xch.res_name = PROGNAME;

	tray_data.tray = XCreateSimpleWindow(tray_data.dpy, RootWindow(tray_data.dpy, DefaultScreen(tray_data.dpy)),
					tray_data.xsh.x, tray_data.xsh.y, 
					tray_data.xsh.width, tray_data.xsh.height, 
					0, settings.bg_color.pixel, settings.bg_color.pixel);

	DBG(8, ("created tray`s window: 0x%x\n", tray_data.tray));

	tray_data.parent = RootWindow(tray_data.dpy, DefaultScreen(tray_data.dpy));

	xswa.bit_gravity = settings.gravity_x;
	XChangeWindowAttributes(tray_data.dpy, tray_data.tray, CWBitGravity, &xswa);

	if (settings.transparent || settings.parent_bg) {
		tray_data.xa_xrootpmap_id = XInternAtom(tray_data.dpy, "_XROOTPMAP_ID", False);
		tray_data.xa_xsetroot_id = XInternAtom(tray_data.dpy, "_XSETROOT_ID", False);
	}
		
	if (settings.parent_bg) {
		XSetWindowBackgroundPixmap(tray_data.dpy, tray_data.tray, ParentRelative);
	}

	if (settings.transparent) {
		if (tray_data.xa_xrootpmap_id == None) {
			DBG(0, ("transparent background could not be supported.\n"));
			DBG(0, ("use compatible tool to set wallpaper --- e.g. Esetroot, fvwm-root, etc.\n"));
		} else {
			update_root_pmap();
			tray_update_bg();
		}
	}

	XmbTextListToTextProperty(tray_data.dpy, &wnd_name, 1, XTextStyle, &wm_name);

	XSetWMProperties(tray_data.dpy, tray_data.tray, &wm_name, NULL, argv, argc, &tray_data.xsh, &xwmh, &xch);
	tray_set_constraints(settings.icon_size, settings.icon_size);

	/* v0.2 tray protocol support */
	orient = settings.vertical ? _NET_SYSTEM_TRAY_ORIENTATION_HORZ : _NET_SYSTEM_TRAY_ORIENTATION_VERT;
	net_system_tray_orientation = XInternAtom(tray_data.dpy, TRAY_ORIENTATION_ATOM, False);
	XChangeProperty(tray_data.dpy, tray_data.tray, 
			net_system_tray_orientation, net_system_tray_orientation, 32, 
			PropModeReplace, 
			(unsigned char *) &orient, 1);

	if (settings.parent_bg)
		XSetWindowBackgroundPixmap(tray_data.dpy, tray_data.tray, ParentRelative);

	XSetWMProtocols(tray_data.dpy, tray_data.tray, &tray_data.xa_wm_delete_window, 1);

	XSelectInput(tray_data.dpy, tray_data.tray, 
			SubstructureNotifyMask | StructureNotifyMask | ExposureMask |
			PropertyChangeMask | SubstructureRedirectMask | VisibilityChangeMask );
#ifndef NO_NATIVE_KDE
	XSelectInput(tray_data.dpy, RootWindow(tray_data.dpy, DefaultScreen(tray_data.dpy)),
			StructureNotifyMask | SubstructureNotifyMask | PropertyChangeMask);
#else
	XSelectInput(tray_data.dpy, RootWindow(tray_data.dpy, DefaultScreen(tray_data.dpy)), PropertyChangeMask);
#endif
}

void tray_set_wm_hints()
{
	int mwm_decor = 0;
	
	if (!settings.hide_wnd_deco) {
		if (!settings.hide_wnd_title)
			mwm_decor |= MWM_DECOR_TITLE | MWM_DECOR_MENU | MWM_DECOR_MINIMIZE | MWM_DECOR_MAXIMIZE;
		if (!settings.hide_wnd_border)
			mwm_decor |= MWM_DECOR_RESIZEH | MWM_DECOR_BORDER;
	}

	mwm_set_hints(tray_data.dpy, tray_data.tray, mwm_decor, MWM_FUNC_ALL);

	if (ewmh_check_support(tray_data.dpy)) {
		if (settings.sticky) 
			ewmh_add_window_state(tray_data.dpy, tray_data.tray, _NET_WM_STATE_STICKY);

		if (settings.skip_taskbar)
			ewmh_add_window_state(tray_data.dpy, tray_data.tray, _NET_WM_STATE_SKIP_TASKBAR);
		
		if (settings.wnd_layer != NULL)
			ewmh_add_window_state(tray_data.dpy, tray_data.tray, settings.wnd_layer);
					
		ewmh_add_window_type(tray_data.dpy, tray_data.tray, _NET_WM_WINDOW_TYPE_NORMAL);
		if (settings.wnd_type != _NET_WM_WINDOW_TYPE_NORMAL)
			ewmh_add_window_type(tray_data.dpy, tray_data.tray, settings.wnd_type);

	}
}

void tray_acquire_selection()
{
	static char *tray_sel_atom_name = NULL;
	Time timestamp;
	
	if (tray_sel_atom_name == NULL) {
		tray_sel_atom_name = (char *)malloc(strlen(TRAY_SEL_ATOM) + 4);
		if (tray_sel_atom_name == NULL) DIE(("out of memory\n"));


		snprintf(tray_sel_atom_name, strlen(TRAY_SEL_ATOM) + 4,
			"%s%u", TRAY_SEL_ATOM, DefaultScreen(tray_data.dpy));
	}

	DBG(8, ("tray_sel_atom_name=%s\n", tray_sel_atom_name));

	tray_data.xa_tray_selection = XInternAtom(tray_data.dpy, tray_sel_atom_name, False);

	tray_data.xa_tray_opcode = XInternAtom(tray_data.dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	tray_data.xa_tray_data = XInternAtom(tray_data.dpy, "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);

	tray_data.xa_xembed = XInternAtom(tray_data.dpy, "_XEMBED", False);

	timestamp = get_server_timestamp(tray_data.dpy, tray_data.tray);
	
	XSetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection, 
			tray_data.tray, timestamp);

	if (XGetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection) != tray_data.tray) {
		DIE(("could not set selection owner.\nMay be another (greedy) tray running?\n"));
	} else {
		tray_data.active = True;
		DBG(3, ("ok, got _NET_SYSTEM_TRAY selection\n"));
	}
	
	send_client_msg32(tray_data.dpy, DefaultRootWindow(tray_data.dpy),
	                  DefaultRootWindow(tray_data.dpy),
	                  XInternAtom(tray_data.dpy, "MANAGER", False),
	                  timestamp,
					  tray_data.xa_tray_selection, tray_data.tray, 0, 0);	
}


