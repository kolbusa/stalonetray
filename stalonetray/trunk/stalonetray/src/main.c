/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* main.c
* Tue, 24 Aug 2004 12:19:48 +0700
* -------------------------------
* main is main
* -------------------------------*/

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/X.h>

#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include "config.h"

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

static int tray_status_requested = 0;

/****************************
 * Signal handlers, cleanup
 ***************************/

void request_tray_status_on_signal(int sig)
{
	psignal(sig, NULL);
	tray_status_requested = 1;
	/* This message is not handled, instead it will
	 * force event loop to spin and eventually reach
	 * periodic tasks handler, which will read
	 * tray_status_requestd flag and print out 
	 * tray status */
	x11_send_client_msg32(tray_data.async_dpy, 
			tray_data.tray, 
			tray_data.tray, 
			tray_data.xa_tray_opcode, 
			0, 
			STALONE_TRAY_STATUS_REQUESTED,
			0, 0, 0);
	/* Force event delivery */
	XSync(tray_data.async_dpy, False);
}

void cleanup()
{
	static int clean = 0;
	static int cleanup_in_progress = 0;

	if (!clean && cleanup_in_progress) {
		ERR(("forced to die\n"));
		abort();
	}
	if (clean) return;
	cleanup_in_progress = 1;
	DBG(6, ("start\n"));

	if (x11_connection_status()) {
		DBG(8, ("being nice to the icons\n"));
		/* Clean the list unembedding icons one by one */
		icon_list_clean_callback(&embedder_unembed);
		/* Give away the selection */
		if (tray_data.is_active) 
			XSetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection, None, CurrentTime);
		/* Sync in order to wait until all icons finish their reparenting
		 * process */
		XSync(tray_data.dpy, False);
		XCloseDisplay(tray_data.dpy);
		XCloseDisplay(tray_data.async_dpy);
	}
	cleanup_in_progress = 0;
	clean = 1;
	DBG(3, ("done\n"));
}

void dump_core_on_signal(int sig)
{
#ifdef DEBUG
# ifdef HAVE_BACKTRACE
#   define BACKTRACE_LEN	15
	void *array[BACKTRACE_LEN];
	size_t size;
# endif
#endif
	psignal(sig, NULL);
#ifdef DEBUG
	DBG(0, ("-- backtrace --\n"));
# if defined(HAVE_BACKTRACE)
	size = backtrace(array, BACKTRACE_LEN);
	DBG(0, ("%d stack frames obtained\n", size));
	DBG(0, ("printing out backtrace\n"));
	backtrace_symbols_fd(array, size, STDERR_FILENO);
# elif defined(HAVE_PRINTSTACK)
	printstack(STDERR_FILENO);
# endif
#endif
	abort();
}

void exit_on_signal(int sig)
{
	psignal(sig, NULL);
	/* This is UGLY and is, probably, to be submitted to
	 * Daily WTF, but it is the only way I found not to 
	 * use usleep in main event loop. */
	DBG(8, ("Sending fake WM_DELETE_WINDOW message\n"));
	x11_send_client_msg32(tray_data.async_dpy, tray_data.tray, tray_data.tray, tray_data.xa_wm_protocols, tray_data.xa_wm_delete_window, 0, 0, 0, 0);
	XSync(tray_data.async_dpy, False);
}

/**************************************
 * (Un)embedding cycle implementation
 **************************************/

/* Add icon to the tray */
void add_icon(Window w, int cmode)
{
	struct TrayIcon *ti;

	/* Aviod adding duplicates */
	if ((ti = icon_list_find(w)) != NULL) {
		DBG(4, ("ignoring second request to embed 0x%x (requested cmode=%d, current cmode=%d\n",
					w,
					cmode,
					ti->cmode));
		return;
	}
	/* Dear Edsger W. Dijkstra, I see you behind my back =( */
	ti = icon_list_new(w, cmode);
	if (ti == NULL) return;

#ifdef DEBUG
	DBG(6, ("starting embedding process for 0x%x with cmode=%d\n", w, cmode));
	x11_dump_win_info(tray_data.dpy, w);
#endif

	if (!xembed_check_support(ti)) goto failed1;
	
	if (ti->is_xembed_supported)
		ti->is_visible = xembed_get_mapped_state(ti);
	else
		ti->is_visible = True;

	if (ti->is_visible) {
		if (!embedder_reset_size(ti)) goto failed1;
		if (!layout_add(ti)) goto failed1;
	}
	
	if (!xembed_embed(ti)) goto failed1;
	if (!embedder_embed(ti)) goto failed2;

	embedder_update_positions(False);
	tray_update_window_size();

	DBG(0, ("0x%x: icon added as %s\n", ti->wid, ti->is_visible ? "visible" : "hidden"));
#ifdef DEBUG
	print_icon_data(ti);
#endif
	return;

failed2:
	layout_remove(ti);
failed1:
#ifdef DEBUG
	DBG(0, ("0x%x: embedding failed\n", ti->wid));
	print_icon_data(ti);	
#endif
	icon_list_free(ti);
	return;
}

/* Remove icon from the tray */
void remove_icon(Window w)
{
	struct TrayIcon *ti;
	
	if ((ti = icon_list_find(w)) == NULL) return;

	embedder_unembed(ti);
	xembed_unembed(ti);
	layout_remove(ti);
	icon_list_free(ti);

	DBG(0, ("0x%x: icon removed\n", w));

	embedder_update_positions(False);
	tray_update_window_size();
}

/* Track icon visibility state changes */
void icon_track_visibility_changes(Window w)
{
	struct TrayIcon *ti;
	int mapped;

	/* Ignore false alarms */
	if ((ti = icon_list_find(w)) == NULL || !ti->is_xembed_supported) return;

	mapped = xembed_get_mapped_state(ti);

	DBG(8, ("xembed_is_mapped(0x%x) = %u\n", w, mapped));
	DBG(8, ("is_visible = %u\n", ti->is_visible));
#ifdef DEBUG
	x11_dump_win_info(tray_data.dpy, ti->wid);
#endif

	/* Nothing has changed */
	if (mapped == ti->is_visible) return;

	ti->is_visible = mapped;
	DBG(0, ("0x%x: %s\n", w, mapped ? "showing" : "hiding"));

	if (mapped) { /* Icon has become mapped and is listed as hidden. Show this icon. */
		embedder_reset_size(ti);
		if (!layout_add(ti)) {
			xembed_set_mapped_state(ti, False);
			return;
		}
		embedder_show(ti);
	} else { /* Icon has become unmapped and is listed as visible. Hide this icon. */
		layout_remove(ti);
		embedder_hide(ti);
	}
	embedder_update_positions(False);
	tray_update_window_size();
}

/* helper to identify invalid icons */
int find_invalid_icons(struct TrayIcon *ti)
{
	return ti->is_invalid;
}

#ifndef NO_NATIVE_KDE
/* Find newly available KDE icons and add them as necessary */
/* TODO: move to kde_tray.c */
void kde_icons_update()
{
	unsigned long list_len, i;
	Window *kde_tray_icons;

	if (tray_data.kde_tray_old_mode || 
		!x11_get_root_winlist_prop(tray_data.dpy, tray_data.xa_kde_net_system_tray_windows, 
				(unsigned char **) &kde_tray_icons, &list_len)) 
	{
		return;
	}

	for (i = 0; i < list_len; i++) 
		/* If the icon is not None and is non old, try to add it
		 * (if the icon is already there, nothing is gonna happen). */
		if (kde_tray_icons[i] != None && !kde_tray_is_old_icon(kde_tray_icons[i])) 
		{
			DBG(8, ("(possibly unembedded) KDE icon 0x%x\n", kde_tray_icons[i]));
			add_icon(kde_tray_icons[i], CM_KDE);
		}

	XFree(kde_tray_icons);
}
#endif

/* Perform several periodic tasks */
void perform_periodic_tasks()
{
	struct TrayIcon *ti;
	/* 1. Remove all invalid icons */
	while ((ti = icon_list_forall(&find_invalid_icons)) != NULL) {
		DBG(4, ("0x%x is invalid. removing\n", ti->wid));
		remove_icon(ti->wid);
	}
	/* 2. Print tray status if asked to */
	if (tray_status_requested) {
		unsigned int grid_w, grid_h;
		tray_status_requested = 0;
		layout_get_size(&grid_w, &grid_h);

#ifdef DEBUG
#	define SHOWMSG(msg) DBG(0, msg)
#else
#	define SHOWMSG(msg)	print_message_to_stderr msg
#endif

#ifdef DEBUG
		trace_mode = 1;
#endif
		SHOWMSG(("Someone asked for tray status. Here it comes.\n"));
		SHOWMSG(("===================================\n"));
		SHOWMSG(("tray status: %sactive\n", tray_data.is_active ? "" : "not "));
		SHOWMSG(("grid geometry: %dx%d\n", 
					grid_w / settings.icon_size, 
					grid_h / settings.icon_size));
		SHOWMSG(("tray geometry: %dx%d+%d+%d\n",
				tray_data.xsh.width, tray_data.xsh.height,
				tray_data.xsh.x, tray_data.xsh.y));
#ifdef DEBUG
		{
			XWindowAttributes xwa;
			XGetWindowAttributes(tray_data.dpy, tray_data.tray, &xwa);
			SHOWMSG(("real tray geometry: %dx%d+%d+%d\n",
						xwa.width, xwa.height, xwa.x, xwa.y));
		}
		if (tray_data.xembed_data.current)
			SHOWMSG(("icon with focus: 0x%x (pointer %p)\n", tray_data.xembed_data.current->wid, tray_data.xembed_data.current));
		else
			SHOWMSG(("no icon is focused\n"));
#endif
		dump_icon_list();
		SHOWMSG(("===================================\n"));
#ifdef DEBUG
		trace_mode = 0;
#endif
#undef SHOWMSG
	}
	{
		/* KLUDGE TODO: resolve */
		XWindowAttributes xwa;
		XGetWindowAttributes(tray_data.dpy, tray_data.tray, &xwa);
		if (!tray_data.is_reparented && 
				(xwa.width != tray_data.xsh.width || xwa.height != tray_data.xsh.height)) 
		{
			DBG(8, ("KLUDGE: fixing window size (current: %dx%d, required: %dx%d)\n",
						xwa.width, xwa.height,
						tray_data.xsh.width, tray_data.xsh.height));
			tray_update_window_size();
		} 
	}
}

/**********************
 * Event handlers
 **********************/

void expose(XExposeEvent ev)
{
	if (ev.window == tray_data.tray && settings.parent_bg) 
		tray_refresh_window(False);
}

void visibility_notify(XVisibilityEvent ev)
{
#if 0 /* Too much flicker, no real benefit :( */
	if (ev.state == VisibilityUnobscured && ev.window == tray_data.tray) {
		static int local_state = True;
		if (local_state) 
			tray_refresh_window(False);
		local_state = !local_state;
	}
#endif
}

void property_notify(XPropertyEvent ev)
{
#define TRACE_PROPS
#if defined(DEBUG) && defined(TRACE_PROPS)
	char *atom_name;
	atom_name = XGetAtomName(tray_data.dpy, ev.atom);
	DBG(6, ("atom = %s\n", atom_name));
	XFree(atom_name);
#endif
	/* React on wallpaper change */
	if (ev.atom == tray_data.xa_xrootpmap_id || ev.atom == tray_data.xa_xsetroot_id) {
		if (settings.transparent) tray_update_bg(True);
		if (settings.parent_bg || settings.transparent || settings.fuzzy_edges)
			tray_refresh_window(True);
	}
#ifndef NO_NATIVE_KDE
	/* React on change of list of KDE icons */
	if (ev.atom == tray_data.xa_kde_net_system_tray_windows) {
		if (tray_data.is_active) 
			kde_icons_update();
		else 
			DBG(6, ("not updating KDE icons list: tray is not active\n"));
		kde_tray_update_old_icons(tray_data.dpy);
	}
#endif
	/* React on _XEMBED_INFO changes of embedded icons
	 * (currently used to track icon visibility status) */
	if (ev.atom == tray_data.xembed_data.xa_xembed_info) {
		icon_track_visibility_changes(ev.window);
	}
	if (ev.atom == tray_data.xa_net_client_list) {
		Window *windows;
		unsigned long nwindows, rc, i;
		rc = x11_get_root_winlist_prop(tray_data.dpy, 
				tray_data.xa_net_client_list,
				(unsigned char **) &windows,
				&nwindows);
		if (x11_ok() && rc) {
			tray_data.is_reparented = True;
			for(i = 0; i < nwindows; i++) 
				if (windows[i] == tray_data.tray) {
					tray_data.is_reparented = False;
					break;
				}
		}
		DBG(6, ("tray is %sreparented\n", tray_data.is_reparented ? "" : "not "));
	}
}

void reparent_notify(XReparentEvent ev)
{
	struct TrayIcon *ti;

	ti = icon_list_find(ev.window);
	if (ti == NULL) return;

	/* Reparenting out of the tray is one of non-destructive
	 * ways to end XEMBED protocol (see spec) */
	if (ti->is_embedded && ti->mid_parent != ev.parent) {
		DBG(3, ("initiating unembedding for 0x%x\n", ti->wid));
#ifdef DEBUG
		x11_dump_win_info(tray_data.dpy, ev.parent);
		dump_icon_list();
#endif
		remove_icon(ev.window);
#ifdef DEBUG
		dump_icon_list();
#endif
	}
}

void client_message(XClientMessageEvent ev)
{
	int cmode = CM_FDO;
	struct TrayIcon *ti;
#ifdef DEBUG
	/* Print neat message(s) about this event to aid debugging */
	char *msg_type_name;

	msg_type_name = XGetAtomName(tray_data.dpy, ev.message_type);

	if (msg_type_name != NULL) {
		DBG(4, ("message \"%s\"\n", msg_type_name));
		XFree(msg_type_name);
	}

	if (ev.message_type == tray_data.xa_wm_protocols) {
		msg_type_name = XGetAtomName(tray_data.dpy, ev.data.l[0]);
		if (msg_type_name != NULL) {
			DBG(6, ("WM_PROTOCOLS message type: %s\n", msg_type_name));
			XFree(msg_type_name);
		}
	}
#endif

	/* Graceful exit */
	if (ev.message_type == tray_data.xa_wm_protocols &&
		ev.data.l[0] == tray_data.xa_wm_delete_window && 
		ev.window == tray_data.tray)
	{
		DBG(3, ("got WM_DELETE message, will now exit\n"));
		exit(0);
	} 

	/* Handle _NET_SYSTEM_TRAY_* messages */
	if (ev.message_type == tray_data.xa_tray_opcode && tray_data.is_active) {
		DBG(6, ("this is the _NET_SYSTEM_TRAY_OPCODE(%lu) message\n", ev.data.l[1]));
	
		switch (ev.data.l[1]) {
			/* This is the starting point of NET SYSTEM TRAY protocol */
			case SYSTEM_TRAY_REQUEST_DOCK:
				DBG(3, ("dockin' requested by 0x%x, serving in a moment\n", ev.data.l[2]));
#ifndef NO_NATIVE_KDE
				if (kde_tray_check_for_icon(tray_data.dpy, ev.data.l[2])) cmode = CM_KDE;
				if (kde_tray_is_old_icon(ev.data.l[2])) kde_tray_old_icons_remove(ev.data.l[2]);
#endif
				add_icon(ev.data.l[2], cmode);
				break;
			/* This is a special case added by this implementation.
			 * STALONETRAY_TRAY_DOCK_CONFIRMED is sent by stalonetray 
			 * to itself. (see embed.c) */
			case STALONE_TRAY_DOCK_CONFIRMED:
				ti = icon_list_find(ev.data.l[2]);
				if (ti != NULL && !ti->is_embedded) {
					ti->is_embedded = True;
					DBG(3, ("0x%x: embedding confirmed\n", ti->wid));
#ifdef DEBUG
					dump_icon_list();
#endif
				}
				tray_update_window_size();
				break;
			/* We ignore these messages, since we do not show
			 * any baloons anyways */
			case SYSTEM_TRAY_BEGIN_MESSAGE:
			case SYSTEM_TRAY_CANCEL_MESSAGE:
			default:
				break;
		}
	}
#ifdef DEBUG
	if (ev.message_type == tray_data.xa_tray_opcode && !tray_data.is_active)
		DBG(6, ("ignoring _NET_SYSTEM_TRAY_OPCODE(%lu) message: tray not active\n", tray_data.is_active));
#endif
}

void destroy_notify(XDestroyWindowEvent ev)
{
	if (!tray_data.is_active && ev.window == tray_data.old_selection_owner) {
		/* Old tray selection owner was destroyed. Take over selection ownership. */
		tray_acquire_selection();
	} else if (ev.window != tray_data.tray) {
		/* Try to remove icon from the tray */
		remove_icon(ev.window);
#ifndef NO_NATIVE_KDE
	} else if (kde_tray_is_old_icon(ev.window)) {
		/* Since X Server may reuse window ids, remove ev.window
		 * from the list of old KDE icons */
		kde_tray_old_icons_remove(ev.window);
#endif
	}
}

void configure_notify(XConfigureEvent ev)
{
	struct TrayIcon *ti;
	struct Point sz;
	XWindowAttributes xwa;

	if (ev.window == tray_data.tray) {
		/* Tray window was resized */
		/* TODO: distinguish between synthetic and real configure notifies */
		/* TODO: catch rejected configure requests */
		DBG(8, ("window geometry from event: %ux%u+%d+%d\n", ev.width, ev.height, ev.x, ev.y));

		/* Sometimes, configure notifies come too late, so we fetch real geometry ourselves */
		XGetWindowAttributes(tray_data.dpy, tray_data.tray, &xwa);
		x11_get_window_abs_coords(tray_data.dpy, tray_data.tray, &tray_data.xsh.x, &tray_data.xsh.y);
		DBG(8, ("real window geometry: %dx%d+%d+%d\n", xwa.width, xwa.height, tray_data.xsh.x, tray_data.xsh.y));
		tray_data.xsh.width = xwa.width;
		tray_data.xsh.height = xwa.height;

		/* Update icons positions */
		icon_list_forall(&grid2window);
		embedder_update_positions(True);

		/* Adjust window background if necessary */
		tray_update_bg(False);
		tray_refresh_window(True);
	} else if ((ti = icon_list_find(ev.window)) != NULL) {
		/* Some icon has resized its window */
		if (ti->cmode == CM_KDE) {
			/* KDE icons are not allowed to change their size. Reset their size. */
			embedder_reset_size(ti);
			return;
		}
		/* Get new window size */
		if (!x11_get_window_size(tray_data.dpy, ti->wid, &sz.x, &sz.y)) {
			embedder_unembed(ti);
			return;
		}
		DBG(3, ("icon window 0x%x was resized, new size: %ux%u, old size: %ux%u\n", ev.window, 
					sz.x, sz.y, ti->l.wnd_sz.x, ti->l.wnd_sz.y));
		/* Check if the size has really changed */
		if (sz.x == ti->l.wnd_sz.x && sz.y == ti->l.wnd_sz.y) return;
		ti->l.wnd_sz = sz;
		ti->is_resized = True;
		/* Do the job */
#if 0
		if (layout_handle_icon_resize(ti)) {
			embedder_refresh(ti);
#ifdef DEBUG
			print_icon_data(ti);
#endif
			embedder_update_positions(False);
			tray_update_window_size();
		}
#else
		layout_handle_icon_resize(ti);
		embedder_refresh(ti);
#ifdef DEBUG
		print_icon_data(ti);
#endif
		embedder_update_positions(False);
		tray_update_window_size();
#endif
#ifdef DEBUG
		dump_icon_list();
#endif
	}
}

void selection_clear(XSelectionClearEvent ev)
{
	/* Is it _NET_SYSTEM_TRAY selection? */
	if (ev.selection == tray_data.xa_tray_selection) {
		/* Is it us who has lost the selection */
		if (ev.window == tray_data.tray) {
			DBG(0, ("stalonetray has lost _NET_WM_SYSTRAY selection; deactivating\n"));
			tray_data.is_active = False;
			tray_data.old_selection_owner = XGetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection);
			if (!x11_ok()) {
				DBG(0, ("could not get new selection owner. re-acquiring selection\n"));
				tray_acquire_selection();
			};
				
			DBG(4, ("the new selection owner 0x%x\n", tray_data.old_selection_owner));
			XSelectInput(tray_data.dpy, tray_data.old_selection_owner, StructureNotifyMask);
			return;
		} else if (!tray_data.is_active) {
			/* Someone else has lost selection and tray is not active --- take over the selection */
			DBG(0, ("re-reacquiring tray selection\n"));
			tray_acquire_selection();
		} else {
			/* Just in case */
			DBG(0, ("WEIRD: tray is active and someone else has lost tray selection :s\n"));
		}
	}
}

void map_notify(XMapEvent ev)
{
#ifndef NO_NATIVE_KDE
	/* Legacy scheme to handle KDE icons */
	if (tray_data.kde_tray_old_mode) {
		Window w = kde_tray_find_icon(tray_data.dpy, ev.window);
		if (w != None) {
			DBG(4, ("Legacy scheme for KDE icons: detected KDE icon 0x%x. Adding.\n", w));
			add_icon(w, CM_KDE);
			/* TODO: remove some properties to trick ion3 so that it no longer thinks that w is a toplevel. 
			 * Candidates for removal: 
			 * 	- WM_STATE */
		}
	}
#endif
}

void unmap_notify(XUnmapEvent ev)
{
	struct TrayIcon *ti;
	ti = icon_list_find(ev.window);
	if (ti != NULL && !ti->is_invalid && ti->cmode == CM_KDE) {
		/* KLUDGE! sometimes KDE icons occasionally 
		 * unmap their windows, but do _not_ destroy 
		 * them. We map those windows back */
		/* XXX: root cause unidentified */
		DBG(8, ("Unmap KDE icons KLUDGE for 0x%x\n", ti->wid));
		XMapRaised(tray_data.dpy, ti->wid);
		if (!x11_ok()) ti->is_invalid = True;
	}
}

/*********************************************************/
int main(int argc, char** argv)
{
	XEvent		ev;
	
	/* Read settings */
	tray_init();
	read_settings(argc, argv);

	/* Register cleanup and signal handlers */
	atexit(cleanup);

	signal(SIGINT,  &exit_on_signal);
	signal(SIGTERM, &exit_on_signal);
	signal(SIGHUP,  &exit_on_signal);

	signal(SIGSEGV, &dump_core_on_signal);
	signal(SIGQUIT, &dump_core_on_signal);

	signal(SIGUSR1, &request_tray_status_on_signal);
	
	/* Open display */
	if ((tray_data.dpy = XOpenDisplay(settings.display_str)) == NULL ||
		(tray_data.async_dpy = XOpenDisplay(settings.display_str)) == NULL)
	{
		DIE(("could not open display\n"));
	}

	if (settings.xsync)
		XSynchronize(tray_data.dpy, True);

	x11_trap_errors();

	/* Interpret those settings that need a display */
	interpret_settings();

	/* Create and show tray window */
	tray_create_window(argc, argv);
	tray_acquire_selection();
	tray_show_window();
#ifndef NO_NATIVE_KDE
	kde_tray_init(tray_data.dpy);
#endif
	xembed_init();
#ifndef NO_NATIVE_KDE
	kde_icons_update();
#endif

	/* Main event loop */
	while ("my guitar gently wheeps") {
		XNextEvent(tray_data.dpy, &ev);
		xembed_handle_event(ev);
		switch (ev.type) {
		case VisibilityNotify:
			DBG(7, ("VisibilityNotify (0x%x, state=%d)\n", ev.xvisibility.window, ev.xvisibility.state));
			visibility_notify(ev.xvisibility);
			break;
		case Expose:
			DBG(7, ("Expose (0x%x)\n", ev.xexpose.window));
			expose(ev.xexpose);
			break;
		case PropertyNotify:
			DBG(7, ("PropertyNotify(0x%x)\n", ev.xproperty.window));
			property_notify(ev.xproperty);
			break;
		case DestroyNotify:
			DBG(7, ("DestroyNotify(0x%x)\n", ev.xdestroywindow.window));
			destroy_notify(ev.xdestroywindow);
			break;
		case ClientMessage:
			DBG(5, ("ClientMessage(from 0x%x?)\n", ev.xclient.window));
			client_message(ev.xclient);
			break;
		case ConfigureNotify:
			DBG(7, ("ConfigureNotify(0x%x)\n", ev.xconfigure.window));
			configure_notify(ev.xconfigure);
			break;
		case MapNotify:
			DBG(7, ("MapNotify(0x%x)\n", ev.xmap.window));
			map_notify(ev.xmap);
			break;
		case ReparentNotify:
			DBG(5, ("ReparentNotify(0x%x to 0x%x)\n", ev.xreparent.window, ev.xreparent.parent));
			reparent_notify(ev.xreparent);
			break;
		case SelectionClear:
			DBG(5, ("SelectionClear (0x%x has lost selection)\n", ev.xselectionclear.window));
			selection_clear(ev.xselectionclear);
			break;
		case SelectionNotify:
			DBG(5, ("SelectionNotify\n"));
			break;
		case SelectionRequest:
			DBG(5, ("SelectionRequest (from 0x%x to 0x%x)\n", ev.xselectionrequest.requestor, ev.xselectionrequest.owner));
			break;
		case UnmapNotify:
			DBG(7, ("UnmapNotify(0x%x)\n", ev.xunmap.window));
			unmap_notify(ev.xunmap);
			break;
		default:
#if defined(DEBUG) && defined(TRACE_EVENTS)
			DBG(8, ("Unhandled event: %s, serial: %d, window: 0x%x\n", x11_event_names[ev.type], ev.xany.serial, ev.xany.window));
#endif
			break;
		}
		perform_periodic_tasks();
	}
	return 0;
}
