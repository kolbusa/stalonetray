/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* main.c
* Tue, 24 Aug 2004 12:19:48 +0700
* -------------------------------
* main is main
* -------------------------------*/

#include "config.h"

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <execinfo.h>

#ifdef DEBUG
#include <unistd.h>
#endif

#include "debug.h"
#include "icons.h"
#include "layout.h"
#include "embed.h"
#include "xembed.h"

#ifndef NO_NATIVE_KDE
#include "kde_tray.h"
#endif

#include "settings.h"
#include "tray.h"

#define PROGNAME PACKAGE

/* from System Tray Protocol Specification 
 * http://freedesktop.org/Standards/systemtray-spec/systemtray-spec-0.2.html */
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define	TRAY_SEL_ATOM "_NET_SYSTEM_TRAY_S"

#define TRAY_ORIENTATION_ATOM "_NET_SYSTEM_TRAY_ORIENTATION" 

#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0
#define _NET_SYSTEM_TRAY_ORIENTATION_VERT 1

TrayData tray_data = {
	.tray		= None,
	.reparented	= 0,
	.parent		= None,
	.dpy 		= NULL,
	.grow_issued	= 0,
	.xsh		= {
	.flags	= (PSize | PPosition),
	.x		= 100,
	.y		= 100,
	.width	= 10,
	.height = 10
	},
	.root_pmap = None,
	.xa_xrootpmap_id = None,
	.xa_xsetroot_id = None
};

static int clean = 0;
void cleanup()
{
	if (clean) {
		return;
	}
	DBG(3, "cleanup()\n");
	clean_icons(&unembed_window);
	clean = 1;
}

void sigsegv(int sig)
{
#ifdef DEBUG

#define BACKTRACE_LEN	15
	
	void *array[BACKTRACE_LEN];
	size_t size, i;
	char **strings;
#endif

	ERR("Got SIGSEGV, dumping core\n");

#ifdef DEBUG
	DBG(0, "-- backtrace --\n");

	size = backtrace(array, BACKTRACE_LEN);

	DBG(0, "%d stack frames obtained\n", size);
	DBG(0, "printing out backtrace\n");

	backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif
	
	abort();
}

#ifdef DEBUG

int trace_mode = 0;

void sigusr1(int sig)
{
	extern struct Point grid_sz;

	trace_mode = 1;
	DBG(3, "Got SIGUSR1. Here comes tray status\n");
	DBG(3, "===================================\n");
	DBG(3, "grid geometry: %dx%d\n", grid_sz.x, grid_sz.y);
	DBG(3, "tray geometry: %dx%d+%d+%d\n",
			tray_data.xsh.width, tray_data.xsh.height,
			tray_data.xsh.x, tray_data.xsh.y);

	forall_icons(&print_icon_data);	
	DBG(3, "===================================\n");
	trace_mode = 0;
}
#endif

void sigterm(int sig)
{
	ERR("Got SIGTERM, exiting\n");
	cleanup();
	exit(0);
}


void sigkill(int sig)
{
	ERR("Got SIGKILL, exiting\n");
	cleanup();
	exit(0);
}


void sigint(int sig)
{
	fprintf(stderr, "Got SIGINT, exiting\n");
	cleanup();
	exit(0);
}

void get_tray_absolute_coords(int *x, int *y)
{
	*x = 0; *y = 0;
}

/* from FVWM:fvwm/colorset.c:172 */
Pixmap get_root_pixmap(Atom prop)
{
	Atom type;
	int format;
	unsigned long length, after;
	unsigned char *reteval = NULL;
	int ret;
	Pixmap pix = None;
	Window root_window = XRootWindow(tray_data.dpy, DefaultScreen(tray_data.dpy));

	ret = XGetWindowProperty(tray_data.dpy, root_window, prop, 0L, 1L, False, XA_PIXMAP,
			   &type, &format, &length, &after, &reteval);
	if (ret == Success && type == XA_PIXMAP && format == 32 && length == 1 &&
	    after == 0)
	{
		pix = ((Pixmap *)reteval)[0];
	}
	if (reteval)
	{
		XFree(reteval);
	}
	return pix;
}

/* from FVWM:fvwm/colorset.c:195 + optimized */
void update_root_pmap()
{
	static Atom xa_xrootpmap_id = None;
	Atom xa_rootpix;
	unsigned int w = 0, h = 0;
	XID dummy;
	Pixmap pix;
	

	if (xa_xrootpmap_id == None)
	{
		xa_xrootpmap_id = XInternAtom(tray_data.dpy,"_XROOTPMAP_ID", False);
	}
	XSync(tray_data.dpy, False);

	xa_rootpix = tray_data.xa_xrootpmap_id != None ? tray_data.xa_xrootpmap_id : xa_xrootpmap_id;
	
	pix = get_root_pixmap(tray_data.xa_xrootpmap_id);

	if (pix && !XGetGeometry(
		tray_data.dpy, pix, &dummy, (int *)&dummy, (int *)&dummy,
		&w, &h, (unsigned int *)&dummy, (unsigned int *)&dummy))
	{
		DBG(0, "Could not update root background pixmap(%d)\n", trapped_error_code);
		pix = None;
	}

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, "Could not update root background pixmap(%d)\n", trapped_error_code);
		return;
	}
	
	tray_data.root_pmap = pix;
	tray_data.root_pmap_width = w;
	tray_data.root_pmap_height = h;
	
	DBG(4, "Got New Root Pixmap: 0x%lx %i,%i\n", tray_data.root_pmap, w, h);
}

int tray_update_bg()
{
	Pixmap bg_pixmap;
	GC gc;
	XGCValues gc_values;
	
	DBG(3, "Updating tray transparent background\n");

	if (tray_data.root_pmap == None) {
		DBG(0, "No root pixmap found\n");
		return FAILURE;
	}

	trap_errors();

	bg_pixmap = XCreatePixmap(tray_data.dpy, tray_data.tray,
			tray_data.xsh.width, tray_data.xsh.height, 
			DefaultDepth(tray_data.dpy, DefaultScreen(tray_data.dpy)));

	DBG(8, "Created pixmap %x:%dx%d\n", bg_pixmap, tray_data.xsh.width, tray_data.xsh.height);

	gc = XCreateGC(tray_data.dpy, bg_pixmap, 0, &gc_values);
			
	XCopyArea(tray_data.dpy, tray_data.root_pmap, bg_pixmap, gc, 
			tray_data.xsh.x, tray_data.xsh.y, 
			tray_data.xsh.width, tray_data.xsh.height, 0, 0);

	DBG(8, "XCopyArea(0x%x, 0x%x, 0x%x, 0x%x, %d, %d, %d, %d, %d, %d)\n",
			tray_data.dpy, tray_data.root_pmap, bg_pixmap, gc, 
			tray_data.xsh.x, tray_data.xsh.y, 
			tray_data.xsh.width, tray_data.xsh.height, 0, 0);


	if (untrap_errors(tray_data.dpy)) {
		DBG(0, "Could not update background (%d)\n", trapped_error_code);
		return FAILURE;
	}
	
	XSetWindowBackgroundPixmap(tray_data.dpy, tray_data.tray, bg_pixmap);
	XClearWindow(tray_data.dpy, tray_data.tray);

	return SUCCESS;
}

int tray_set_constraints(int width, int height) 
{
	XSizeHints xsh;
	
	xsh.min_width = width;
	xsh.min_height = height;
	
	xsh.width_inc = settings.icon_size;
	xsh.height_inc = settings.icon_size;
	
	xsh.base_width = settings.icon_size;
	xsh.base_height = settings.icon_size;

	xsh.max_width = settings.max_tray_width;
	xsh.max_height = settings.max_tray_height;

	xsh.flags = PResizeInc|PBaseSize|PMinSize|PMaxSize; 

	trap_errors();

	XSetWMNormalHints(tray_data.dpy, tray_data.tray, &xsh);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, "Could not set tray's minimal size(%d)\n", trapped_error_code);
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

	if (!tray_data.reparented) 
		XMoveResizeWindow(tray_data.dpy, tray_data.tray, tray_data.xsh.x, tray_data.xsh.y,
				tray_data.xsh.width, tray_data.xsh.height);
	else
		XResizeWindow(tray_data.dpy, tray_data.tray, tray_data.xsh.width, tray_data.xsh.height);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, "could not move/resize window (%d)\n", trapped_error_code);
		return FAILURE;
	}

	/* XXX we need to be shure, that the size has really changed.
	 * this is dirty but seems to be unavoidable :( */
	/* XXX WE NEED A CONSTANT */
	for(wait_cd = 5; wait_cd >= 0; wait_cd--) {
		XGetWindowAttributes(tray_data.dpy, tray_data.tray, &xwa);

		if (tray_data.xsh.width == xwa.width && tray_data.xsh.height == xwa.height) {
			DBG(3, "ok, tray size updated\n");
			return SUCCESS;
		}
		
		DBG(8, "current geometry: %dx%d, waiting for %dx%d\n",
				xwa.width, xwa.height, tray_data.xsh.width, xwa.height);
		
		if (wait_cd > 0) usleep(50000L);
	}

	DBG(0, "failed to resize the tray, falling to %dx%d size\n", xwa.width, xwa.height);
	tray_data.xsh.width = xwa.width;
	tray_data.xsh.height = xwa.height;
	return SUCCESS;
}

int tray_grow(struct Point dsz)
{
	XSizeHints xsh_old = tray_data.xsh;
	int ok;

	DBG(3, "grow(), dx=%d, dy=%d, grow_gravity=%c\n", dsz.x, dsz.y, settings.grow_gravity);
	DBG(4, "old geometry:%dx%d+%d+%d\n", tray_data.xsh.width, tray_data.xsh.height, tray_data.xsh.x, tray_data.xsh.y);
	
	if (!tray_grow_check(dsz)) {
		return FAILURE;
	}
	
	if (settings.grow_gravity & GRAV_V) {
		tray_data.xsh.height += dsz.y;
		if (settings.grow_gravity & GRAV_S && !tray_data.reparented) {
			tray_data.xsh.y -= dsz.y;
		}
	}

	if (settings.grow_gravity & GRAV_H) {
		tray_data.xsh.width += dsz.x;
		if (settings.grow_gravity & GRAV_E && !tray_data.reparented) {
			tray_data.xsh.x -= dsz.x;
		}
	}
	
	DBG(3, "new geometry:%dx%d+%d+%d\n", tray_data.xsh.width, tray_data.xsh.height, tray_data.xsh.x, tray_data.xsh.y);

	tray_data.grow_issued = 1;
	
	ok = tray_update_size();
	
	if (!ok) 
		tray_data.xsh = xsh_old;

	return SUCCESS;
}

void embed_icon(Window w, int cmode)
{
	struct TrayIcon *ti;

	if ((ti = add_icon(w, cmode)) == NULL) return;
	if (!layout_icon(ti)) goto bail_out;
	if (!embed_window(ti)) goto bail_out_full;

#ifdef DEBUG
	DBG(2, "0x%x: 1st stage ok\n");
	print_icon_data(ti);	
#endif	
	return;

bail_out_full:
	unlayout_icon(ti);
bail_out:
	del_icon(ti);
	return;
}

void unembed_icon(Window w)
{
	struct TrayIcon *ti;
	
	if ((ti = find_icon(w)) == NULL) return;

	unembed_window(ti);
	unlayout_icon(ti);
	del_icon(ti);

	update_positions();
}

/* event handlers */
void property_notify(XPropertyEvent ev)
{
#ifdef DEBUG
	char *atom_name;

	trap_errors();
	
	atom_name = XGetAtomName(tray_data.dpy, ev.atom);

	if (atom_name != NULL) {
		DBG(6, "atom is \"%s\"\n", atom_name);
		XFree(atom_name);
	}

	if (tray_data.xa_xrootpmap_id != None) {
		atom_name = XGetAtomName(tray_data.dpy, tray_data.xa_xrootpmap_id);
		if (atom_name != NULL) {
			DBG(8, "root atom is \"%s\"\n", atom_name);
			XFree(atom_name);
		}
	} else {
		DBG(8, "root atom is None\n");
	}

	untrap_errors(tray_data.dpy);
#endif

	if ((settings.transparent || settings.parent_bg) &&
		(ev.atom == tray_data.xa_xrootpmap_id || ev.atom == tray_data.xa_xsetroot_id)) 
	{
		DBG(8, "Background pixmap has changed.\n");
		if (settings.transparent) {
			update_root_pmap();
			tray_update_bg();
		}

		if (settings.parent_bg) {
			XClearWindow(tray_data.dpy, tray_data.parent);
			XClearWindow(tray_data.dpy, tray_data.tray);
		}
	}
}

void reparent_notify(XReparentEvent ev)
{
	struct TrayIcon *tmp;
	
	if (ev.window == tray_data.tray) {
		DBG(3, "Was reparented (by WM?) to 0x%x\n", ev.parent);
		tray_data.reparented = ev.parent != RootWindow(tray_data.dpy, DefaultScreen(tray_data.dpy));
		tray_data.parent = ev.parent;

		if (settings.transparent)
			tray_update_bg();

		return;
	}

	tmp = find_icon(ev.window);
		
	if (tmp == NULL) return;

	if (tmp->l.p == ev.parent) {
		DBG(3, "0x%x: embedding is accompished\n", tmp->w);
		tmp->embeded = 1;

#ifdef DEBUG
		print_icon_data(tmp);
#endif
		
		return;
	}

	if (!tmp->embeded) { /* XXX */
		DBG(0, "Somebody (0x%x) has reparented into tray ?!!!\n", ev.window);
		return;
	}

	if (tmp->l.p != ev.parent) {
		DBG(3, "Initiating unembedding proccess for 0x%x\n", tmp->w);
		unembed_icon(ev.window);
	}
}

void map_notify(XMapEvent ev)
{
	if (ev.window == tray_data.tray && settings.transparent) {
		tray_update_bg();
		return;
	}

#ifndef NO_NATIVE_KDE
	{
		Window kde_tray_icon;
		struct TrayIcon *tmp;


		tmp = find_icon(ev.window);

		if (tmp != NULL) {
			return;
		}

		kde_tray_icon = check_kde_tray_icons(tray_data.dpy, ev.window);

		if (kde_tray_icon != None) {
			embed_icon(kde_tray_icon, CM_KDE);
			return;
		}
	}
#endif

}

void xembed_message(XClientMessageEvent ev)
{
}

void expose(XExposeEvent ev)
{
	if (ev.window == tray_data.tray && settings.transparent)
		tray_update_bg();
}

void client_message(XClientMessageEvent ev)
{
#ifdef DEBUG
	char *msg_type_name;

	msg_type_name = XGetAtomName(tray_data.dpy, ev.message_type);

	if (msg_type_name != NULL) {
		DBG(3, "message_type is \"%s\"\n", msg_type_name);
		XFree(msg_type_name);
	}
#endif
	
	if (ev.message_type == tray_data.xa_wm_protocols && /* wm */
		ev.data.l[0] == tray_data.xa_wm_delete_window)
	{
		DBG(8, "WM_DELETE. Will now exit\n");
		exit(0);
	}

	if (ev.message_type == tray_data.xa_tray_opcode) {
		DBG(3, "_NET_SYSTEM_TRAY_OPCODE(%u) message\n", ev.data.l[1]);
		switch (ev.data.l[1]) {
			case SYSTEM_TRAY_REQUEST_DOCK:
				embed_icon(ev.data.l[2], CM_FDO);
				break;
			case SYSTEM_TRAY_BEGIN_MESSAGE:
			case SYSTEM_TRAY_CANCEL_MESSAGE:
			default:
				break;
		}
	}

	if (ev.message_type == tray_data.xa_xembed) {
		DBG(2, "_XEMBED message\n");
		xembed_message(ev);
	}
	
}

void destroy_notify(XDestroyWindowEvent ev)
{
	if (ev.window != tray_data.tray) {
		unembed_icon(ev.window);
	}	
}

void configure_notify(XConfigureEvent ev)
{
	struct TrayIcon *ti;
	
	if (ev.window == tray_data.tray) {
		DBG(4, "geometry from event: %ux%u+%d+%d\n", ev.width, ev.height, ev.x, ev.y);

		if (settings.transparent)
			tray_update_bg();

		/* if ev.send event is set, then the message
		 * came from WM and contains trays coords */
		if (ev.send_event) { 
			DBG(4, "updating coords\n");
			tray_data.xsh.x = ev.x;
			tray_data.xsh.y = ev.y;
		}

#if 0 
		/* this is for future */
		if (tray_data.grow_issued) {
			update_positions();
			tray_data.grow_issued = 0;
			return;
		}
#endif

		tray_data.xsh.width = ev.width;
		tray_data.xsh.height = ev.height;

		DBG(4, "new tray geometry: %ux%u+%d+%d\n", 
				tray_data.xsh.width, tray_data.xsh.height, tray_data.xsh.x, tray_data.xsh.y);

	} else {
		ti = find_icon(ev.window);
		if (ti != NULL) {
			DBG(0, "icon 0x%x has resized itself\n", ev.window);
			layout_handle_icon_resize(ti);
			update_positions();
		}
	}

}

void create_tray_window(int argc, char **argv)
{
	char *wnd_name = PROGNAME;
	XTextProperty wm_name;

	XSetWindowAttributes xswa;
	
	XClassHint	xch;
	XWMHints	xwmh = {
		flags:	AllHints, /*(InputHint | StateHint),*/
		input:	True,
		initial_state: NormalState,
		icon_pixmap: None,
		icon_window: None,
		icon_x: 0,
		icon_y: 0,
		icon_mask: None,
		window_group: 0	
	};

	long		mwm_hints[5] = {2, 0, 0, 0, 0};
	Atom		mwm_wm_hints, net_system_tray_orientation;
	CARD32 		orient;

	tray_data.xa_wm_delete_window = XInternAtom(tray_data.dpy, "WM_DELETE_WINDOW", False);
	tray_data.xa_wm_take_focus = XInternAtom(tray_data.dpy, "WM_TAKE_FOCUS", False);
	tray_data.xa_wm_protocols = XInternAtom(tray_data.dpy, "WM_PROTOCOLS", False);

	xch.res_class = PROGNAME;
	xch.res_name = PROGNAME;

	tray_data.tray = XCreateSimpleWindow(tray_data.dpy, RootWindow(tray_data.dpy, DefaultScreen(tray_data.dpy)),
					tray_data.xsh.x, tray_data.xsh.y, 
					tray_data.xsh.width, tray_data.xsh.height, 
					0, settings.bg_color.pixel, settings.bg_color.pixel);

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
			DBG(0, "Transparent background could not be supported.\n");
			DBG(0, "Use compatable tool to set wallpaper --- e.g. Esetroot, fvwm-root, etc.\n");
		} else {
			update_root_pmap();
			tray_update_bg();
		}
	}

	XmbTextListToTextProperty(tray_data.dpy, &wnd_name, 1, XTextStyle, &wm_name);

	XSetWMProperties(tray_data.dpy, tray_data.tray, &wm_name, NULL, argv, argc, &tray_data.xsh, &xwmh, &xch);
	tray_set_constraints(settings.icon_size, settings.icon_size);

	mwm_wm_hints = XInternAtom(tray_data.dpy, "_MOTIF_WM_HINTS", False);
	mwm_hints[2] = (settings.hide_wnd_deco == 0);
	XChangeProperty(tray_data.dpy, tray_data.tray, mwm_wm_hints, mwm_wm_hints, 32, 0, 
			(unsigned char *) mwm_hints, sizeof(mwm_hints));	

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
			SubstructureNotifyMask | StructureNotifyMask | 
			PropertyChangeMask | SubstructureRedirectMask );
#ifndef NO_NATIVE_KDE
	XSelectInput(tray_data.dpy, RootWindow(tray_data.dpy, DefaultScreen(tray_data.dpy)),
			StructureNotifyMask | SubstructureNotifyMask | PropertyChangeMask);
#else
	XSelectInput(tray_data.dpy, RootWindow(tray_data.dpy, DefaultScreen(tray_data.dpy)), PropertyChangeMask);
#endif
}

void acquire_tray_selection()
{
	char *tray_sel_atom_name;
	
	if ((tray_sel_atom_name = (char *)malloc(strlen(TRAY_SEL_ATOM) + 2)) == NULL)
		DIE("out of memory\n");

	snprintf(tray_sel_atom_name, strlen(TRAY_SEL_ATOM) + 2,
		"%s%u", TRAY_SEL_ATOM, DefaultScreen(tray_data.dpy));

	DBG(8, "tray_sel_atom_name=%s\n", tray_sel_atom_name);

	tray_data.xa_tray_selection = XInternAtom(tray_data.dpy, tray_sel_atom_name, False);

	free(tray_sel_atom_name);

	tray_data.xa_tray_opcode = XInternAtom(tray_data.dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	tray_data.xa_tray_data = XInternAtom(tray_data.dpy, "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);

	tray_data.xa_xembed = XInternAtom(tray_data.dpy, "_XEMBED", False);

	XSetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection, 
			tray_data.tray, CurrentTime);

	if (XGetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection) != tray_data.tray) {
		DIE("could not set selection owner.\nMay be another (greedy) tray running?\n");
	} else {
		DBG(3, "OK. Got _NET_SYSTEM_TRAY selection\n");
	}
	
	xclient_msg32(tray_data.dpy, RootWindow(tray_data.dpy, DefaultScreen(tray_data.dpy)),
					XInternAtom(tray_data.dpy, "MANAGER", False),
					CurrentTime, tray_data.xa_tray_selection, tray_data.tray, 0, 0);	
}

int main(int argc, char** argv)
{


	XWindowAttributes xwa;
	XEvent		ev;
	XColor		xc_bg = {
		.flags	= 0,
		.pad	= 0,
		.pixel	= 0x777777,
		.red	= 0,
		.green	= 0,
		.blue	= 0
	};

	char		*tray_sel_atom_name;

	read_settings(argc, argv);
	
	/* register cleanups */
	atexit(cleanup);
	signal(SIGKILL, &sigkill);
	signal(SIGTERM, &sigterm);
	signal(SIGINT, &sigint);
	signal(SIGSEGV, &sigsegv);
#ifdef DEBUG
	signal(SIGUSR1, &sigusr1);
#endif
	/* here comes main stuff */
	if ((tray_data.dpy = XOpenDisplay(settings.display_str)) == NULL)
		DIE("could not open display\n");

	if (settings.xsync)
		XSynchronize(tray_data.dpy, True);

	interpret_settings();

	create_tray_window(argc, argv);

	acquire_tray_selection();

	XMapRaised(tray_data.dpy, tray_data.tray);
	XMoveWindow(tray_data.dpy, tray_data.tray, tray_data.xsh.x, tray_data.xsh.y);
	XFlush(tray_data.dpy);

	for(;;) {
		while (XPending(tray_data.dpy)) {
			XNextEvent(tray_data.dpy, &ev);
			switch (ev.type) {
			case Expose:
				DBG(7, "Expose(0x%x)\n", ev.xexpose.window);
				expose(ev.xexpose);
				break;
			case MapNotify:
				DBG(5, "MapNotify(0x%x)\n", ev.xmap.window);
				map_notify(ev.xmap);
				break;
			case PropertyNotify:
				DBG(7, "PropertyNotify(0x%x)\n", ev.xproperty.window);
				property_notify(ev.xproperty);
				break;
			case DestroyNotify:
				DBG(7, "DestroyNotyfy(0x%x)\n", ev.xdestroywindow.window);
				destroy_notify(ev.xdestroywindow);
				break;
			case ClientMessage:
				DBG(2, "ClientMessage(from 0x%x?)\n", ev.xclient.window);
				client_message(ev.xclient);
				break;
			case ConfigureNotify:
				DBG(7, "ConfigureNotify(0x%x)\n", ev.xconfigure.window);
				configure_notify(ev.xconfigure);
				break;
			case ReparentNotify:
				DBG(5, "ReparentNotify(0x%x to 0x%x)\n", ev.xreparent.window, ev.xreparent.parent);
				reparent_notify(ev.xreparent);
				break;
			case SelectionClear:
				DBG(5, "SelectionClear (0x%x lost selection)\n", ev.xselectionclear.window);
				break;
			case SelectionNotify:
				DBG(5, "SelectionNotify\n");
				break;
			case SelectionRequest:
				DBG(5, "SelectionRequest (from 0x%x to 0x%x)\n", ev.xselectionrequest.requestor, ev.xselectionrequest.owner);
				break;
			default:
				DBG(8, "Got event, type: %d\n", ev.type);
				break;
			}
		}
		usleep(50000L);
	}

	return 0;
}
