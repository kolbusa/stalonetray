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
#include "xutils.h"

#ifndef NO_NATIVE_KDE
#include "kde_tray.h"
#endif

#include "settings.h"
#include "tray.h"

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

/****************************
 * Signal handlers, cleaup
 ***************************/

static int clean = 0;
void cleanup()
{
	if (clean) {
		return;
	}
	DBG(3, "cleanup()\n");
	clean_icons(&unembed_window);

	/* Give away selection */
	if (tray_data.active) 
		XSetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection, None, CurrentTime);
	
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

/**************************************
 * (Un)embedding cycle implementation
 **************************************/

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

/**********************
 * Event handlers
 **********************/

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

		if (tmp != NULL || !tray_data.active) {
			return;
		}

		kde_tray_icon = check_kde_tray_icons(tray_data.dpy, ev.window);

		if (kde_tray_icon != None && find_icon(kde_tray_icon) == NULL) {
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
	int cmode = CM_FDO;
#ifndef NO_NATIVE_KDE
	Window kde_tray_wnd;
#endif
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

	if (ev.message_type == tray_data.xa_tray_opcode && tray_data.active) {
		DBG(3, "_NET_SYSTEM_TRAY_OPCODE(%u) message\n", ev.data.l[1]);
		
#ifndef NO_NATIVE_KDE
		kde_tray_wnd = check_kde_tray_icons(tray_data.dpy, ev.data.l[2]);
		if (kde_tray_wnd == ev.data.l[2]) cmode = CM_KDE;
#endif	
		
		switch (ev.data.l[1]) {
			case SYSTEM_TRAY_REQUEST_DOCK:
				embed_icon(ev.data.l[2], cmode);
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
	if (settings.permanent && ev.window == tray_data.old_sel_owner) {
		tray_acquire_selection();
	} else if (ev.window != tray_data.tray) {
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

void selection_clear(XSelectionClearEvent ev)
{
	if (ev.selection == tray_data.xa_tray_selection) {
		if (ev.window == tray_data.tray) {
			/* Oh! We've lost it all :( */
			DBG(0, "tray has lost selection. deactivating\n");
			tray_data.active = False;

			trap_errors();

			tray_data.old_sel_owner = XGetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection);
	
			if (untrap_errors(tray_data.dpy)) {
				DBG(0, "could not get new selection owner. permanent behaviour disabled\n");
				settings.permanent = False;
			} else {
				DBG(4, "new selection owner 0x%x\n", tray_data.old_sel_owner);
				XSelectInput(tray_data.dpy, tray_data.old_sel_owner, StructureNotifyMask);
			}	
			return;
		} else if (!tray_data.active) {
			tray_acquire_selection();
			DBG(0, "reacquire tray selection: %s\n", (tray_data.active ? "success" : "failed"));
		} else {
			DBG(0, "WEIRD: tray is active and someone else has lost tray selection :s\n");
		}
	}
}

/*********************************************************/

int main(int argc, char** argv)
{
	XEvent		ev;

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
	tray_create_window(argc, argv);
	tray_acquire_selection();

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
				DBG(5, "SelectionClear (0x%x has lost selection)\n", ev.xselectionclear.window);
				selection_clear(ev.xselectionclear);
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