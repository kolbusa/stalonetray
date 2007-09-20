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

#ifdef DEBUG
#include <unistd.h>
#if defined(HAVE_BACKTRACE)
	#include <execinfo.h>
#elif defined(HAVE_PRINTSTACK)
	#include <ucontext.h>
#endif
#endif

#include "common.h"
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

struct TrayData tray_data;

/****************************
 * Signal handlers, cleanup
 ***************************/

static int clean = 0;
void cleanup()
{
	if (clean) {
		return;
	}
	DBG(3, ("cleanup()\n"));
	clean_icons(&unembed_window);

	/* Give away the selection */
	if (tray_data.active) 
		XSetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection, None, CurrentTime);
	
	clean = 1;
}

void sigsegv(int sig)
{
#ifdef DEBUG

#ifdef HAVE_BACKTRACE

#define BACKTRACE_LEN	15
	
	void *array[BACKTRACE_LEN];
	size_t size;
#endif

#endif

	ERR(("Got SIGSEGV, dumping core\n"));

#ifdef DEBUG
	DBG(0, ("-- backtrace --\n"));

#if defined(HAVE_BACKTRACE)

	size = backtrace(array, BACKTRACE_LEN);

	DBG(0, ("%d stack frames obtained\n", size));
	DBG(0, ("printing out backtrace\n"));

	backtrace_symbols_fd(array, size, STDERR_FILENO);
#elif defined(HAVE_PRINTSTACK)
	printstack(STDERR_FILENO);
#endif
#endif
	
	abort();
}


void sigusr1(int sig)
{
	extern struct Point grid_sz;

#ifdef DEBUG
	trace_mode = 1;
	DBG(3, ("Got SIGUSR1. Here comes the tray status\n"));
	DBG(3, ("===================================\n"));
	DBG(3, ("grid geometry: %dx%d\n", grid_sz.x, grid_sz.y));
	DBG(3, ("tray geometry: %dx%d+%d+%d\n",
			tray_data.xsh.width, tray_data.xsh.height,
			tray_data.xsh.x, tray_data.xsh.y));

	forall_icons(&print_icon_data);	
	DBG(3, ("===================================\n"));
	trace_mode = 0;
#else
	fprintf(stderr, "Got SIGUSR1. Here comes the tray status.\n");
	fprintf(stderr, "==================================\n");
	fprintf(stderr, "grid geometry: %dx%d\n", grid_sz.x, grid_sz.y);
	fprintf(stderr, "tray geometry: %dx%d+%d+%d\n",
			tray_data.xsh.width, tray_data.xsh.height,
			tray_data.xsh.x, tray_data.xsh.y);

	forall_icons(&print_icon_data);

	fprintf(stderr, "==================================\n");
#endif
}

void sigterm(int sig)
{
	ERR(("Got SIGTERM, exiting\n"));
	cleanup();
	exit(0);
}


void sigkill(int sig)
{
	ERR(("Got SIGKILL, exiting\n"));
	cleanup();
	exit(0);
}


void sigint(int sig)
{
	ERR(("Got SIGINT, exiting\n"));
	cleanup();
	exit(0);
}

/**************************************
 * (Un)embedding cycle implementation
 **************************************/

int unembed_if_invalid(struct TrayIcon *ti)
{
	if (ti->invalid) unembed_window(ti);
	return NO_MATCH;
}

void embed_icon(Window w, int cmode)
{
	struct TrayIcon *ti;

	static struct Point kde_icn_sz = { 22, 22 };
	static struct Point def_icn_sz = { FALLBACK_SIZE, FALLBACK_SIZE };

	if ((ti = add_icon(w, cmode)) == NULL) return;

	if (!(set_window_size(ti, cmode == CM_KDE ? kde_icn_sz : def_icn_sz))) goto failed1;

	if (!layout_icon(ti)) goto failed1;
	if (!embed_window(ti)) goto failed2;

	update_positions();
	forall_icons(&unembed_if_invalid);
	
	tray_update_size_hints();
#ifdef DEBUG
	DBG(2, ("0x%x: embedded\n", ti->w));
	print_icon_data(ti);	
#endif
	return;

failed2:
	unlayout_icon(ti);
failed1:
	del_icon(ti);
#ifdef DEBUG
	DBG(2, ("0x%x: embedding failed\n", ti->w));
	print_icon_data(ti);	
#endif
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
	forall_icons(&unembed_if_invalid);
	tray_update_size_hints();
}

#ifndef NO_NATIVE_KDE
void embed_kde_icons()
{
	unsigned long list_len, i;
	Window *kde_tray_icons;

	collect_kde_tray_icons(tray_data.dpy, &kde_tray_icons, &list_len);

	for (i = 0; i < list_len; i++) 
		if (kde_tray_icons[i] != None) {
			trap_errors();

#if 0
			XSelectInput(tray_data.dpy, kde_tray_icons[i], StructureNotifyMask);
			XUnmapWindow(tray_data.dpy, kde_tray_icons[i]);
/*			wait_for_event(kde_tray_icons[i], UnmapNotify);*/
			XReparentWindow(tray_data.dpy, kde_tray_icons[i], DefaultRootWindow(tray_data.dpy), 0, 0);
/*			wait_for_event(kde_tray_icons[i], ReparentNotify);*/
			XMapRaised(tray_data.dpy, kde_tray_icons[i]);
			wait_for_event(kde_tray_icons[i], MapNotify);
#endif

			if (untrap_errors(tray_data.dpy)) {
				DBG(0, ("Could not embed KDE icon 0x%x (%d)", kde_tray_icons[i], trapped_error_code));
			} else {
				embed_icon(kde_tray_icons[i], CM_KDE);
			}
		}
}
#endif

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
		DBG(6, ("atom name: \"%s\"\n", atom_name));
		XFree(atom_name);
	}

	untrap_errors(tray_data.dpy);
#endif

	if (settings.transparent && (ev.atom == tray_data.xa_xrootpmap_id || 
	                             ev.atom == tray_data.xa_xsetroot_id     )) 
	{
		DBG(8, ("background pixmap has changed.\n"));

		if (settings.transparent) {
			update_root_pmap();
			tray_update_bg();
		}
	}
}

void reparent_notify(XReparentEvent ev)
{
	struct TrayIcon *tmp;

	tmp = find_icon(ev.window);
		
	if (tmp == NULL) return;

#if 0
#ifdef DEBUG
	dump_window_info(ev.parent);
#endif

	DBG(8, ("0x%x: reparent_notify.serial = %u (%u)\n", tmp->w, ev.serial, tmp->reparent_serial));

#endif
	
	if (ev.parent == tmp->l.p && !tmp->embedded) {
		tmp->embedded = True;
		DBG(3, ("0x%x: embedding confirmed\n", tmp->w));
		return;
	}

	/* XXX: check timestamp !!!!! */
	/* XXX: needs review (why not for KDE icons?) */
	/*if (tmp->embedded && tmp->l.p != ev.parent && tmp->cmode != CM_KDE) {*/
	if (tmp->embedded && tmp->l.p != ev.parent) {
		DBG(3, ("initiating the unembedding process for 0x%x\n", tmp->w));
		unembed_icon(ev.window);
	}
}

void map_notify(XMapEvent ev)
{

#ifndef NO_NATIVE_KDE
	if (tray_data.active) {
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
	struct TrayIcon *ti;

	DBG(8, ("this is the _XEMBED message,window: 0x%x, opcode: %u, detail: 0x%x, data1 = 0x%x, data2 = 0x%x\n",
	        ev.window, ev.data.l[1], ev.data.l[2], ev.data.l[3], ev.data.l[4]));

	if ((ti = find_icon(ev.window)) == NULL) return;

	if (ev.data.l[1] == XEMBED_REQUEST_FOCUS ||
	    ev.data.l[1] == XEMBED_FOCUS_NEXT    ||
		ev.data.l[1] == XEMBED_FOCUS_PREV      )
	{
		struct TrayIcon *tgt;
		unsigned long focus;

		if (tray_data.xembed_current != None) 
			xembed_focus_out(tray_data.dpy, ti->w);

		switch (ev.data.l[1]) {
		case XEMBED_REQUEST_FOCUS:
			tgt = ti;
			focus = XEMBED_FOCUS_CURRENT;
			break;
		case XEMBED_FOCUS_NEXT:
			tgt = next_icon(ti);
			focus = XEMBED_FOCUS_FIRST;
			break;
		case XEMBED_FOCUS_PREV:
			tgt = prev_icon(ti);
			focus = XEMBED_FOCUS_LAST;
			break;
		}
		
		/* XXX */
		xembed_focus_in(tray_data.dpy, ti->w, focus);
		tray_data.xembed_current = tgt;
	}

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
		DBG(3, ("message name: \"%s\"\n", msg_type_name));
		XFree(msg_type_name);
	}
#endif
	
	if (ev.message_type == tray_data.xa_wm_protocols && /* wm */
		ev.data.l[0] == tray_data.xa_wm_delete_window)
	{
		DBG(8, ("this is the WM_DELETE message, will now exit\n"));
		exit(0);
	}

	if (ev.message_type == tray_data.xa_tray_opcode && tray_data.active) {
		DBG(3, ("this is the _NET_SYSTEM_TRAY_OPCODE(%lu) message\n", ev.data.l[1]));
		
#ifndef NO_NATIVE_KDE
		kde_tray_wnd = check_kde_tray_icons(tray_data.dpy, ev.data.l[2]);
		if (kde_tray_wnd == ev.data.l[2]) cmode = CM_KDE;
#endif	
		
		switch (ev.data.l[1]) {
			case SYSTEM_TRAY_REQUEST_DOCK:
				DBG(3, ("dockin' requested, serving in a moment\n"));
				embed_icon(ev.data.l[2], cmode);
				break;
			case SYSTEM_TRAY_BEGIN_MESSAGE:
			case SYSTEM_TRAY_CANCEL_MESSAGE:
			default:
				break;
		}
	}

	if (ev.message_type == tray_data.xa_xembed)
		xembed_message(ev);
}

void destroy_notify(XDestroyWindowEvent ev)
{
	if (!tray_data.active && settings.permanent && ev.window == tray_data.old_sel_owner) {
		tray_acquire_selection();
	} else if (ev.window != tray_data.tray) {
		unembed_icon(ev.window);

	}
}

void configure_notify(XConfigureEvent ev)
{
	struct TrayIcon *ti;
	struct Point sz;
	int x = 0, y = 0;

	if (ev.window == tray_data.tray) {
		DBG(4, ("the geometry from the event: %ux%u+%d+%d\n", ev.width, ev.height, ev.x, ev.y));
		
		if (get_window_abs_coords(tray_data.dpy, tray_data.tray, &x, &y)) {
			DBG(4, ("absolute position: +%d+%d\n", x, y));
			tray_data.xsh.x = x;
			tray_data.xsh.y = y;
		} else 
			DBG(0, ("could not get tray`s absolute coords\n"));

		tray_data.xsh.width = ev.width;
		tray_data.xsh.height = ev.height;

		DBG(4, ("the new tray geometry: %ux%u+%d+%d\n", 
				tray_data.xsh.width, tray_data.xsh.height, tray_data.xsh.x, tray_data.xsh.y));

		if (settings.transparent)
			tray_update_bg();

	} else {
		ti = find_icon(ev.window);
		if (ti != NULL) {
			/* get new window size */
			if (!get_window_size(ti, &sz)) {
				unembed_window(ti);
				return;
			}
			/* check if the size has really chaned */
			if (sz.x == ti->l.wnd_sz.x && sz.y == ti->l.wnd_sz.y) return;
			ti->l.wnd_sz = sz;
			ti->l.resized = True;
			DBG(0, ("icon 0x%x has resized itself\n", ev.window));
			
			/* do the job */
			if (layout_handle_icon_resize(ti)) {
				if (!show_window(ti)) {
					unlayout_icon(ti); /* or hide ? */
					return;
				}
#ifdef DEBUG
				print_icon_data(ti);
#endif
				update_positions();
				forall_icons(&unembed_if_invalid);
				tray_update_size_hints();
			} else {
				/* Leave icon alone as it was... */
#if 0	
				hide_window(ti);
#endif
			}
#ifdef DEBUG	
			DBG(4, ("====================\n"));
			forall_icons(&print_icon_data);
			DBG(4, ("====================\n"));
#endif
		}
	}
}

void selection_clear(XSelectionClearEvent ev)
{
	if (ev.selection == tray_data.xa_tray_selection) {
		if (ev.window == tray_data.tray) {
			/* Oh! We've lost it all :( */
			DBG(0, ("the tray has lost it's selection. deactivating\n"));
			tray_data.active = False;

			trap_errors();

			tray_data.old_sel_owner = XGetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection);
	
			if (untrap_errors(tray_data.dpy)) {
				DBG(0, ("could not get new selection owner. permanent behaviour disabled\n"));
				settings.permanent = False;
			} else {
				DBG(4, ("the new selection owner 0x%x\n", tray_data.old_sel_owner));
				XSelectInput(tray_data.dpy, tray_data.old_sel_owner, StructureNotifyMask);
			}	
			return;
		} else if (!tray_data.active) {
			tray_acquire_selection();
			DBG(0, ("reacquire tray selection: %s\n", (tray_data.active ? "success" : "failed")));
		} else {
			DBG(0, ("WEIRD: tray is active and someone else has lost tray selection :s\n"));
		}
	}
}

void focus_change(XFocusChangeEvent ev)
{
	if (ev.type == FocusIn)
}

/*********************************************************/

int main(int argc, char** argv)
{
	XEvent		ev;

	tray_init_data();

	read_settings(argc, argv);
	
	/* register cleanups */
	atexit(cleanup);
	signal(SIGKILL, &sigkill);
	signal(SIGTERM, &sigterm);
	signal(SIGINT, &sigint);
	signal(SIGSEGV, &sigsegv);
	signal(SIGUSR1, &sigusr1);
	
	/* here comes main stuff */
	if ((tray_data.dpy = XOpenDisplay(settings.display_str)) == NULL)
		DIE(("could not open display\n"));

	if (settings.xsync)
		XSynchronize(tray_data.dpy, True);

	interpret_settings();
	tray_create_window(argc, argv);
	tray_acquire_selection();
	tray_show();

	XFlush(tray_data.dpy);
	XSync(tray_data.dpy, False);

	if (tray_data.old_sel_owner == None) embed_kde_icons();

	for(;;) {
		while (XPending(tray_data.dpy)) {
			XNextEvent(tray_data.dpy, &ev);
			switch (ev.type) {
			case Expose:
				DBG(7, ("Expose(0x%x)\n", ev.xexpose.window));
				expose(ev.xexpose);
				break;
			case MapNotify:
				DBG(5, ("MapNotify(0x%x)\n", ev.xmap.window));
				map_notify(ev.xmap);
				break;
			case PropertyNotify:
				DBG(7, ("PropertyNotify(0x%x)\n", ev.xproperty.window));
				property_notify(ev.xproperty);
				tray_data.time = ev.xproperty.time;
				break;
			case DestroyNotify:
				DBG(7, ("DestroyNotyfy(0x%x)\n", ev.xdestroywindow.window));
				destroy_notify(ev.xdestroywindow);
				break;
			case ClientMessage:
				DBG(2, ("ClientMessage(from 0x%x?)\n", ev.xclient.window));
				client_message(ev.xclient);
				break;
			case ConfigureNotify:
				DBG(7, ("ConfigureNotify(0x%x)\n", ev.xconfigure.window));
				configure_notify(ev.xconfigure);
				break;
			case FocusIn:
			case FocusOut:
				focus_change(ev.xfocus);
				break;
			case ReparentNotify:
				DBG(5, ("ReparentNotify(0x%x to 0x%x)\n", ev.xreparent.window, ev.xreparent.parent));
				reparent_notify(ev.xreparent);
				break;
			case SelectionClear:
				DBG(5, ("SelectionClear (0x%x has lost selection)\n", ev.xselectionclear.window));
				selection_clear(ev.xselectionclear);
				tray_data.time = ev.xselectionclear.time;
				break;
			case SelectionNotify:
				DBG(5, ("SelectionNotify\n"));
				tray_data.time = ev.xselection.time;
				break;
			case SelectionRequest:
				DBG(5, ("SelectionRequest (from 0x%x to 0x%x)\n", ev.xselectionrequest.requestor, ev.xselectionrequest.owner));
				tray_data.time = ev.xselectionrequest.time;
				break;
			default:
				DBG(8, ("Got event, type: %d\n", ev.type));
				break;
			}
		}
		usleep(5000L);
	}

	return 0;
}
