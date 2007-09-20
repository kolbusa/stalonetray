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

#define TRACE_EVENTS

struct TrayData tray_data;

/****************************
 * Signal handlers, cleanup
 ***************************/

static int clean = 0;
void cleanup()
{
	if (clean) return;
	DBG(3, ("cleanup: start\n"));
	clean_icons_cbk(&unembed_window);
	/* Give away the selection */
	trap_errors();
	if (tray_data.active) 
		XSetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection, None, CurrentTime);
	untrap_errors(tray_data.dpy);
	clean = 1;
	DBG(3, ("cleanup: done\n"));
}

void sigsegv(int sig)
{
#ifdef DEBUG
# ifdef HAVE_BACKTRACE
#   define BACKTRACE_LEN	15
	void *array[BACKTRACE_LEN];
	size_t size;
# endif
#endif
	ERR(("Got SIGSEGV, dumping core\n"));
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
	if (tray_data.xembed_data.current)
		DBG(3, ("icon with focus: 0x%x (pointer %p)\n", tray_data.xembed_data.current->wid, tray_data.xembed_data.current));
	else
		DBG(3, ("no icon is focused\n"));
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

#ifdef DEBUG
void sigusr2(int sig)
{
	static int mark_num = 0;
	DBG(0, ("MARK %d -------------------------------------------------\n", mark_num++));
}
#endif

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

int force_icon_size(struct TrayIcon *ti)
{
	static struct Point kde_icn_sz = { 22, 22 };
	static struct Point def_icn_sz;
	if (ti->is_size_set && ti->cmode != CM_KDE)
		return SUCCESS;
	def_icn_sz.x = settings.icon_size;
	def_icn_sz.y = settings.icon_size;
	ti->is_size_set = True;
	return set_window_size(ti, ti->cmode == CM_KDE ? kde_icn_sz : def_icn_sz);
}

void add_icon(Window w, int cmode)
{
	struct TrayIcon *ti;

	if ((ti = find_icon(w)) != NULL) return;
	/* Edsger W. Dijkstra, I see you behind my back =( */
	if ((ti = allocate_icon(w, cmode)) == NULL) return;
	if (!xembed_check_support(ti)) goto failed1;
	if (!ti->is_hidden) {
		if (!force_icon_size(ti)) goto failed1;
		if (!layout_icon(ti)) goto failed1;
	}
	if (!xembed_embed(ti)) goto failed1;
	if (!embed_window(ti)) goto failed2;
	update_positions();
	tray_update_size_hints();
	DBG(2, ("0x%x: icon added as %s\n", ti->wid, ti->is_hidden ? "hidden" : "visible"));
#ifdef DEBUG
	print_icon_data(ti);
#endif
	return;

failed2:
	unlayout_icon(ti);
failed1:
	deallocate_icon(ti);
#ifdef DEBUG
	DBG(2, ("0x%x: embedding failed\n", ti->wid));
	print_icon_data(ti);	
#endif
	return;
}

void remove_icon(Window w)
{
	struct TrayIcon *ti;
	
	if ((ti = find_icon(w)) == NULL) return;

	unembed_window(ti);
	xembed_unembed(ti);
	unlayout_icon(ti);
	deallocate_icon(ti);

	update_positions();
	tray_update_size_hints();
}

void showhide_icon(Window w)
{
	struct TrayIcon *ti;
	int mapped;
	if ((ti = find_icon(w)) == NULL) return;
	mapped = xembed_is_mapped(ti);
	DBG(6, ("xembed_is_mapped(0x%x) = %u\n", w, mapped));
	DBG(6, ("is_hidden = %u\n", ti->is_hidden));
	if (mapped && ti->is_hidden) { /*show*/
		DBG(6, ("showing 0x%x\n", w));
		force_icon_size(ti);
		if (!layout_icon(ti)) {
			xembed_hide(ti);
			return;
		}
		show_window(ti);
		update_positions();
		tray_update_size_hints();
	} else if (!mapped && !ti->is_hidden) { /*hide*/
		DBG(6, ("hiding 0x%x\n", w));
		unlayout_icon(ti);
		hide_window(ti);
		update_positions();
		tray_update_size_hints();
	}
}

int find_invalid_icons(struct TrayIcon *ti)
{
	return ti->is_invalid;
}

#ifndef NO_NATIVE_KDE
void kde_icons_update()
{
	unsigned long list_len, i;
	Window *kde_tray_icons;

	if (!get_root_window_prop(tray_data.dpy, tray_data.xa_kde_net_system_tray_windows, &kde_tray_icons, &list_len)) 
		return;

	for (i = 0; i < list_len; i++) 
		if (kde_tray_icons[i] != None && !is_old_kde_tray_icon(kde_tray_icons[i])) 
		{
			DBG(9, ("KDE icon 0x%x\n", kde_tray_icons[i]));
			add_icon(kde_tray_icons[i], CM_KDE);
		}

	XFree(kde_tray_icons);
}
#endif

void perform_periodic_tasks()
{
	struct TrayIcon *ti;
	while ((ti = forall_icons(&find_invalid_icons)) != NULL) {
		DBG(4, ("0x%x is invalid. removing\n", ti->wid));
		remove_icon(ti->wid);
	}
}

/**********************
 * Event handlers
 **********************/

void property_notify(XPropertyEvent ev)
{
#define TRACE_PROPS
#if defined(DEBUG) && defined(TRACE_PROPS)
	char *atom_name;
	atom_name = XGetAtomName(tray_data.dpy, ev.atom);
	DBG(6, ("property_notify: atom = %s\n", atom_name));
	XFree(atom_name);
#endif
	if (settings.transparent && (ev.atom == tray_data.xa_xrootpmap_id || 
	                             ev.atom == tray_data.xa_xsetroot_id )) 
	{
		if (settings.transparent) {
			update_root_pmap();
			tray_update_bg();
		}
	}
#ifndef NO_NATIVE_KDE
	if (ev.atom == tray_data.xa_kde_net_system_tray_windows) {
		kde_icons_update();
		update_old_kde_icons(tray_data.dpy);
	}
#endif
	if (ev.atom == tray_data.xembed_data.xa_xembed_info) {
		showhide_icon(ev.window);
	}
}

void reparent_notify(XReparentEvent ev)
{
	struct TrayIcon *ti;

	ti = find_icon(ev.window);
	if (ti == NULL) return;

	if (ti->is_embedded && ti->mid_parent != ev.parent) {
		DBG(3, ("initiating the unembedding process for 0x%x\n", ti->wid));
#ifdef DEBUG
		dump_win_info(tray_data.dpy, ev.parent);
		forall_icons(&print_icon_data);
#endif
		remove_icon(ev.window);
#ifdef DEBUG
		forall_icons(&print_icon_data);
#endif
	}
}

void client_message(XClientMessageEvent ev)
{
	int cmode = CM_FDO;
	struct TrayIcon *ti;
#ifdef DEBUG
	char *msg_type_name;

	msg_type_name = XGetAtomName(tray_data.dpy, ev.message_type);

	if (msg_type_name != NULL) {
		DBG(3, ("message name: \"%s\"\n", msg_type_name));
		XFree(msg_type_name);
	}

	if (ev.message_type == tray_data.xa_wm_protocols) {
		msg_type_name = XGetAtomName(tray_data.dpy, ev.data.l[0]);
		if (msg_type_name != NULL) {
			DBG(5, ("WM_PROTOCOLS message type: %s\n", msg_type_name));
			XFree(msg_type_name);
		}
	}
#endif
	
	if (ev.message_type == tray_data.xa_wm_protocols &&
		ev.data.l[0] == tray_data.xa_wm_delete_window)
	{
		DBG(8, ("this is the WM_DELETE message, will now exit\n"));
		exit(0);
	} 

	if (ev.message_type == tray_data.xa_tray_opcode && tray_data.active) {
		DBG(3, ("this is the _NET_SYSTEM_TRAY_OPCODE(%lu) message\n", ev.data.l[1]));
	
		switch (ev.data.l[1]) {
			case SYSTEM_TRAY_REQUEST_DOCK:
				DBG(3, ("dockin' requested by 0x%x, serving in a moment\n", ev.data.l[2]));
#ifdef DEBUG
				dump_win_info(tray_data.dpy, ev.data.l[2]);
#endif
#ifndef NO_NATIVE_KDE
				if (is_kde_tray_icon(tray_data.dpy, ev.data.l[2])) cmode = CM_KDE;
				if (is_old_kde_tray_icon(ev.data.l[2])) unmark_old_kde_tray_icon(ev.data.l[2]);
#endif
				add_icon(ev.data.l[2], cmode);
				break;
			case STALONE_TRAY_DOCK_CONFIRMED:
				ti = find_icon(ev.data.l[2]);
				if (ti != NULL && !ti->is_embedded) {
					ti->is_embedded = True;
					DBG(3, ("0x%x: embedding confirmed\n", ti->wid));
				}
				break;
			case SYSTEM_TRAY_BEGIN_MESSAGE:
			case SYSTEM_TRAY_CANCEL_MESSAGE:
			default:
				break;
		}
	}
}

void destroy_notify(XDestroyWindowEvent ev)
{
	if (!tray_data.active && ev.window == tray_data.old_sel_owner) {
		tray_acquire_selection();
	} else if (ev.window != tray_data.tray) {
		remove_icon(ev.window);
#ifndef NO_NATIVE_KDE
	} else if (is_old_kde_tray_icon(ev.window)) {
		unmark_old_kde_tray_icon(ev.window);
#endif
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

		forall_icons(&grid2window);
		update_positions_forced();
		tray_update_bg();

	} else {
		ti = find_icon(ev.window);
		if (ti != NULL) {
			if (ti->cmode == CM_KDE) {
				force_icon_size(ti);
				return;
			}
			/* get new window size */
			if (!get_window_size(ti, &sz)) {
				unembed_window(ti);
				return;
			}
			DBG(0, ("icon 0x%x has resized itsel, new size: %ux%u, old size: %ux%u\n", ev.window, 
						sz.x, sz.y, ti->l.wnd_sz.x, ti->l.wnd_sz.y));
			/* check if the size has really chaned */
			if (sz.x == ti->l.wnd_sz.x && sz.y == ti->l.wnd_sz.y) return;
			ti->l.wnd_sz = sz;
			ti->is_resized = True;
			/* do the job */
			if (layout_handle_icon_resize(ti)) {
				repaint_icon(ti);
#ifdef DEBUG
				print_icon_data(ti);
#endif
				update_positions();
				tray_update_size_hints();
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
			DBG(0, ("stalonetray has lost it's _NET_WM_SYSTRAY selection. deactivating\n"));
			tray_data.active = False;
			trap_errors();
			tray_data.old_sel_owner = XGetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection);
			if (untrap_errors(tray_data.dpy)) {
				DBG(0, ("could not get new selection owner. reacquiering selection\n"));
				tray_acquire_selection();
			};
				
			DBG(4, ("the new selection owner 0x%x\n", tray_data.old_sel_owner));
			trap_errors();
			XSelectInput(tray_data.dpy, tray_data.old_sel_owner, StructureNotifyMask);
			untrap_errors(tray_data.dpy);
			return;
		} else if (!tray_data.active) {
			tray_acquire_selection();
			DBG(0, ("reacquiring tray selection: %s\n", (tray_data.active ? "success" : "failed")));
		} else {
			DBG(0, ("WEIRD: tray is active and someone else has lost tray selection :s\n"));
		}
	}
}

/*********************************************************/

int main(int argc, char** argv)
{
	XEvent		ev;
	tray_init_data();
	read_settings(argc, argv);
	
	atexit(cleanup);
	signal(SIGKILL, &sigkill);
	signal(SIGTERM, &sigterm);
	signal(SIGINT, &sigint);
	signal(SIGSEGV, &sigsegv);
	signal(SIGUSR1, &sigusr1);
#ifdef DEBUG
	signal(SIGUSR2, &sigusr2);
#endif
	
	if ((tray_data.dpy = XOpenDisplay(settings.display_str)) == NULL)
		DIE(("could not open display\n"));

	if (settings.xsync)
		XSynchronize(tray_data.dpy, True);

	interpret_settings();
	tray_create_window(argc, argv);
	tray_acquire_selection();
	tray_show();
#ifndef NO_NATIVE_KDE
	kde_tray_support_init(tray_data.dpy);
#endif
	xembed_init();
#ifndef NO_NATIVE_KDE
	kde_icons_update();
#endif

	trap_errors();

	while ("my guitar gently wheeps") {
		while (XPending(tray_data.dpy)) {
			XNextEvent(tray_data.dpy, &ev);
			xembed_handle_event(ev);
			switch (ev.type) {
			case PropertyNotify:
				DBG(7, ("PropertyNotify(0x%x)\n", ev.xproperty.window));
				property_notify(ev.xproperty);
				break;
			case DestroyNotify:
				DBG(7, ("DestroyNotify(0x%x)\n", ev.xdestroywindow.window));
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
			default:
#if defined(DEBUG) && defined(TRACE_EVENTS)
				DBG(9, ("Unhandled event: %s, serial: %d\n", get_event_name(ev.type), ev.xany.serial));
#endif
				break;
			}
			perform_periodic_tasks();
		}
		usleep(5000L);
	}
	return 0;
}
