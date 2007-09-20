/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* xembed.c
* Tue, 24 Aug 2004 12:05:38 +0700
* -------------------------------
* XEMBED protocol implementation
* -------------------------------*/

/* BIG FAT FIXME: currently tab order is
 * not very dependant on the visual order of 
 * icons. This MUST be fixed */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "common.h"
#include "debug.h"
#include "xembed.h"
#include "xutils.h"
#include "wmh.h"

#include "list.h"

#define XEMBED_ERROR					0xffffffff

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY			0
#define XEMBED_WINDOW_ACTIVATE  		1
#define XEMBED_WINDOW_DEACTIVATE  		2
#define XEMBED_REQUEST_FOCUS		 	3
#define XEMBED_FOCUS_IN 	 			4
#define XEMBED_FOCUS_OUT  				5
#define XEMBED_FOCUS_NEXT 				6	
#define XEMBED_FOCUS_PREV 				7
/* 8-9 were used for XEMBED_GRAB_KEY/XEMBED_UNGRAB_KEY */
#define XEMBED_MODALITY_ON 				10
#define XEMBED_MODALITY_OFF 			11
#define XEMBED_REGISTER_ACCELERATOR     12
#define XEMBED_UNREGISTER_ACCELERATOR   13
#define XEMBED_ACTIVATE_ACCELERATOR     14

/* Details for  XEMBED_FOCUS_IN: */
#define XEMBED_FOCUS_CURRENT			0
#define XEMBED_FOCUS_FIRST 				1
#define XEMBED_FOCUS_LAST				2

/* mods field for XEMBED_REGISTER_ACCELERATOR */
#define XEMBED_MODIFIER_SHIFT			(1 << 0)
#define XEMBED_MODIFIER_CONTROL			(1 << 1)
#define XEMBED_MODIFIER_ALT				(1 << 2)
#define XEMBED_MODIFIER_SUPER			(1 << 3)
#define XEMBED_MODIFIER_HYPER			(1 << 4)

/* Flags for XEMBED_ACTIVATE_ACCELERATOR */
#define XEMBED_ACCELERATOR_OVERLOADED	(1 << 0)

/* Directions for focusing */
#define XEMBED_DIRECTION_DEFAULT		0
#define XEMBED_DIRECTION_UP_DOWN		1
#define XEMBED_DIRECTION_LEFT_RIGHT		2

/* Flags for _XEMBED_INFO */
#define XEMBED_MAPPED					(1 << 0)

unsigned long xembed_get_info(Display *dpy, Window dst, unsigned long *version);
int xembed_embedded_notify(Display *dpy, Window src, Window dst, long timestamp);
int xembed_window_activate(Display *dpy, Window dst, long timestamp);
int xembed_window_deactivate(Display *dpy, Window dst, long timestamp);
int xembed_focus_in(Display *dpy, Window dst, long focus, long timestamp);
int xembed_focus_out(Display *dpy, Window dst, long timestamp);
int xembed_activate_accelerator(Display *dpy, Window dst, long timestamp, long id, long overloaded);

struct TrayIcon *xembed_next_icon();
struct TrayIcon *xembed_prev_icon();

int xembed_process_kbd_event(XKeyEvent xkey);
void xembed_add_accel(struct TrayIcon *owner, long id, long symb, long mods);
void xembed_del_accel(long id);
void xembed_act_accel(long id);
void xembed_switch_focus_to(struct TrayIcon *tgt, long focus);

void xembed_track_focus_change(XFocusChangeEvent ev);
void xembed_message(XClientMessageEvent ev);

void xembed_init()
{
	XSizeHints xsh;
	/* 1. Initialize data structures */
	tray_data.xembed_data.window_has_focus = False;
	tray_data.xembed_data.current = NULL;
	tray_data.xembed_data.accels = NULL;
	tray_data.xembed_data.timestamp = CurrentTime;
	/* 2. Create focus proxy (see XEMBED spec) */
	tray_data.xembed_data.focus_proxy = 
		XCreateSimpleWindow(tray_data.dpy, RootWindow(tray_data.dpy, 
		                    DefaultScreen(tray_data.dpy)), -1, -1, 1, 1, 0, 0, 0);
	DBG(8, ("created focus proxy, wid=0x%x\n", tray_data.xembed_data.focus_proxy));
	xsh.x = -1; xsh.y = -1; xsh.width = 1; xsh.height = 1; xsh.flags = USPosition | USSize | PPosition | PSize;
	XSetNormalHints(tray_data.dpy, tray_data.xembed_data.focus_proxy, &xsh);
	XSelectInput(tray_data.dpy, tray_data.xembed_data.focus_proxy, FocusChangeMask);
	/* 3. Hide focus proxy window from the WM */
	if (ewmh_check_support(tray_data.dpy)) {
		ewmh_add_window_state(tray_data.dpy, tray_data.xembed_data.focus_proxy, _NET_WM_STATE_SKIP_TASKBAR);
		ewmh_add_window_state(tray_data.dpy, tray_data.xembed_data.focus_proxy, _NET_WM_STATE_SKIP_PAGER);
	}
	XMapRaised(tray_data.dpy, tray_data.xembed_data.focus_proxy);
}

/* FIXME: split into multiple subroutines */
void xembed_handle_event(XEvent ev)
{
	switch (ev.type) {
	case FocusIn:
	case FocusOut:
		if (ev.xfocus.window == tray_data.xembed_data.focus_proxy)		
			xembed_track_focus_change(ev.xfocus);
		break;
	case ClientMessage:
		if (ev.xclient.message_type == tray_data.xa_xembed) {
			xembed_message(ev.xclient);
		} else if (ev.xclient.message_type == tray_data.xa_wm_protocols &&
		           ev.xclient.data.l[0] == tray_data.xa_wm_take_focus) 
		{
			trap_errors();
			tray_data.xembed_data.timestamp = ev.xclient.data.l[1];
			XSetInputFocus(tray_data.dpy, tray_data.xembed_data.focus_proxy, RevertToNone, ev.xclient.data.l[1]);
			if (untrap_errors(tray_data.dpy)) {
#ifdef DEBUG
				DIE(("could not set input focus to proxy window (maybe it's gone?). dying\n"));
#else
				DIE(("internal error.\n"
				     "please consider building a debug version if the problem persists\n"
					 "and send a mail to the author (see AUTHORS file)\n"));
#endif
			}
		} else if (ev.xclient.message_type == tray_data.xa_tray_opcode) {
			/* we peek at _NET_SYSTEM_TRAY_OPCODE messages 
			 * to obtain proper timestamp for embedding */
			tray_data.xembed_data.timestamp = ev.xclient.data.l[0];
		}
		break;
	case KeyRelease:
	case KeyPress:
		tray_data.xembed_data.timestamp = ev.xkey.time;
		if (ev.type == KeyRelease && xembed_process_kbd_event(ev.xkey))
			break;
		if (tray_data.xembed_data.current != NULL) {
			trap_errors();
			XSendEvent(tray_data.dpy, tray_data.xembed_data.current->w, False, NoEventMask, &ev);
			XSync(tray_data.dpy, False);
			if (untrap_errors(tray_data.dpy)) {
				/* XXX: someone should act on this... */
				tray_data.xembed_data.current->invalid = True;
			}
			DBG(9, ("Sent key event to 0x%x\n", tray_data.xembed_data.current->w));
		}
		break;
	}
}

void xembed_embed(struct TrayIcon *ti)
{
	unsigned long flags, version;
	flags = xembed_get_info(tray_data.dpy, ti->w, &version);

	DBG(8, ("timestamp = %u\n", tray_data.xembed_data.timestamp));

	ti->xembed_supported = flags != XEMBED_ERROR;
	if (!ti->xembed_supported) return;

	ti->xembed_wants_focus = True;
	xembed_embedded_notify(tray_data.dpy, tray_data.tray, ti->w, tray_data.xembed_data.timestamp);

	ti->xembed_last_timestamp = tray_data.xembed_data.timestamp;
	ti->xembed_last_msgid = XEMBED_EMBEDDED_NOTIFY;

	if (tray_data.xembed_data.current == NULL) {
		tray_data.xembed_data.current = ti;
		xembed_focus_in(tray_data.dpy, ti->w, XEMBED_FOCUS_FIRST, tray_data.xembed_data.timestamp);
	}
	
	if (tray_data.xembed_data.window_has_focus) 
		xembed_window_activate(tray_data.dpy, ti->w, tray_data.xembed_data.timestamp);
}

void xembed_unembed(struct TrayIcon *ti)
{
	struct TrayIcon *tmp;
	tray_data.xembed_data.timestamp = get_server_timestamp(tray_data.dpy, tray_data.tray);
	if (ti == tray_data.xembed_data.current) {
		tmp = xembed_next_icon(ti);
		if (tmp == ti || tmp->xembed_wants_focus == False) {
			xembed_switch_focus_to(NULL, 0);
		} else {
			xembed_switch_focus_to(tmp, XEMBED_FOCUS_FIRST);
		}
	}
}

/*********** implementation level ***************/

void xembed_switch_focus_to(struct TrayIcon *tgt, long focus)
{
	DBG(5, ("tgt=%p, focus=%d\n", tgt, focus));
	if (tray_data.xembed_data.current != NULL) {
		DBG(5, ("focus removed from 0x%x (pointer %p)\n", tray_data.xembed_data.current->w, tray_data.xembed_data.current));
		xembed_focus_out(tray_data.dpy, tray_data.xembed_data.current->w, tray_data.xembed_data.timestamp);
	}

	if (tgt != NULL) {
		xembed_focus_in(tray_data.dpy, tgt->w, focus, tray_data.xembed_data.timestamp);
		DBG(5, ("focus set to 0x%x (pointer %p)\n", tgt->w, tgt));
	} else
		DBG(5, ("focus set no none\n"));

	tray_data.xembed_data.current = tgt;
}

static int activate = 0;

int broadcast_activate_msg(struct TrayIcon *ti)
{
	if (activate)
		xembed_window_activate(tray_data.dpy, ti->w, tray_data.xembed_data.timestamp);
	else
		xembed_window_deactivate(tray_data.dpy, ti->w, tray_data.xembed_data.timestamp);

	return NO_MATCH;
}

void xembed_track_focus_change(XFocusChangeEvent ev)
{
	tray_data.xembed_data.window_has_focus = (ev.type == FocusIn);
	activate = tray_data.xembed_data.window_has_focus;
	forall_icons(&broadcast_activate_msg);
}

void xembed_message(XClientMessageEvent ev)
{
	struct TrayIcon *ti, *tgt;
	unsigned long focus;
	long msgid;

	DBG(5, ("this is the _XEMBED message, window: 0x%x, timestamp: %u, opcode: %u, \ndetail: 0x%x, data1 = 0x%x, data2 = 0x%x\n",
	        ev.window, ev.data.l[0], ev.data.l[1], ev.data.l[2], ev.data.l[3], ev.data.l[4]));
#if DEBUG
	if (tray_data.xembed_data.current != NULL) 
		DBG(8, ("xembed_data.current = %p (window: 0x%x)\n", tray_data.xembed_data.current, tray_data.xembed_data.current->w));
	else
		DBG(8, ("XEMBED focus is unset\n"));
#endif

	if ((ti = find_icon_ex(ev.window)) == NULL) return;
	if (!ti->xembed_supported) {
		DBG(1, ("WEIRD: refusing to handle an XEMBED message from the icon which does not support XEMBED...\n"));
		return;
	}

	if (ev.data.l[0] == CurrentTime) 
		ev.data.l[0] = get_server_timestamp(tray_data.dpy, tray_data.tray);

	tray_data.xembed_data.timestamp = ev.data.l[0];
	msgid = ev.data.l[1];

	switch (msgid) {
	case XEMBED_REQUEST_FOCUS:
		tgt = ti;
		DBG(5, ("new focus target: 0x%x (pointer %p)\n", tgt->w, tgt));
		ti->xembed_wants_focus = True;
		xembed_switch_focus_to(tgt, XEMBED_FOCUS_FIRST);
		ti->xembed_last_timestamp = tray_data.xembed_data.timestamp;
		ti->xembed_last_msgid = msgid;
		break;
	case XEMBED_FOCUS_NEXT:
	case XEMBED_FOCUS_PREV:
		while (1) {
			if (msgid == XEMBED_FOCUS_NEXT) {
				tgt = xembed_next_icon(ti);
				focus = XEMBED_FOCUS_FIRST;
			} else {
				tgt = xembed_prev_icon(ti);
				focus = XEMBED_FOCUS_LAST;
			}
			DBG(5, ("new focus target: 0x%x (pointer %p)\n", tgt->w, tgt));
			if (tgt->xembed_wants_focus == False) 
			{
				xembed_switch_focus_to(NULL, 0);
				break;
			}
			else if (tgt->xembed_last_timestamp == tray_data.xembed_data.timestamp &&
			         (tgt->xembed_last_msgid == XEMBED_FOCUS_NEXT ||
			          tgt->xembed_last_msgid == XEMBED_FOCUS_PREV))
			{
				DBG(5, ("focus disabled for 0x%x (pointer %p)\n", tgt->w, tgt));
				tgt->xembed_wants_focus = False;
				ti = tgt;
			} 
			else 
			{
				xembed_switch_focus_to(tgt, focus);
				break;
			}
		}
		ti->xembed_last_timestamp = tray_data.xembed_data.timestamp;
		ti->xembed_last_msgid = msgid;
#if 0
		if (tgt->xembed_wants_focus == False) {
			xembed_focus_out(tray_data.dpy, ti->w, tray_data.xembed_data.timestamp);
			tray_data.xembed_data.current = NULL;
			goto bail_out;
		}

		if (tgt == tray_data.xembed_data.current &&
		    tray_data.xembed_data.timestamp == tgt->last_xembed_timestamp &&
			msgid == tgt->last_xembed_msgid) 
		{
			xembed_focus_out(tray_data.dpy, ti->w, tray_data.xembed_data.timestamp);
			tray_data.xembed_data.current = NULL;
			goto bail_out;
		}
		if (tgt == tray_data.xembed_data.current) {
		/* XXX: this is questionable behaviour.... */
			tray_data.xembed_data.current = NULL;
			return;
		};
		if (tray_data.xembed_data.current != None) 
			xembed_focus_out(tray_data.dpy, ti->w, tray_data.xembed_data.timestamp);

		xembed_focus_in(tray_data.dpy, tgt->w, focus, tray_data.xembed_data.timestamp);
		tray_data.xembed_data.current = tgt;
#endif
		break;
	case XEMBED_REGISTER_ACCELERATOR:
		xembed_add_accel(ti, ev.data.l[2], ev.data.l[3], ev.data.l[4]);
		break;
	case XEMBED_UNREGISTER_ACCELERATOR:
		xembed_del_accel(ev.data.l[2]);
		break;
	default:
		DBG(0, ("UNHANDLED XEMBED message, id = %d\n", ev.data.l[1]));
		break;
	}
}

/* FIXME: ACHTUNG: this implementation assumes that accelerator ids never coincide.
 * Naiive, to say the least... */

void xembed_add_accel(struct TrayIcon* owner, long id, long symb, long mods)
{
	struct XEMBEDAccel *xaccel, *tmp;
	xaccel = (struct XEMBEDAccel *) malloc(sizeof(struct XEMBEDAccel));
	if (xaccel == NULL) 
		DIE(("OOM while registering XEMBED accelerator\n"));

	xaccel->owner = owner;
	xaccel->id = id;
	xaccel->symb = symb;
	xaccel->mods = mods;
	xaccel->overloaded = 0;

	for (tmp = tray_data.xembed_data.accels; tmp != NULL; tmp = tmp->next)
		if (tmp->symb == symb && tmp->mods == mods) {
			xaccel->overloaded++;
			tmp->overloaded++;
		}

	LIST_ADD_ITEM(tray_data.xembed_data.accels, xaccel);
	DBG(8, ("added accel: owner=0x%x, id=0x%x, sym=0x%x, mods=0x%x, overloaded=%d\n", 
	        owner, id, symb, mods, xaccel->overloaded));
}

void xembed_del_accel(long id)
{
	struct XEMBEDAccel *tmp, *tgt = NULL;
	for (tmp = tray_data.xembed_data.accels; tmp != NULL; tmp = tmp->next)
		if (tmp->id == id) {
			tgt = tmp;
			return;
		}
	if (tgt == NULL) {
		DBG(1, ("request to remove unregistered accelerator\n"));
		return;
	}
	for (tmp = tray_data.xembed_data.accels; tmp != NULL; tmp = tmp->next)
		if (tmp->symb == tgt->symb && tmp->mods == tgt->mods) 
			tmp->overloaded--;
	LIST_DEL_ITEM(tray_data.xembed_data.accels, tgt);
	DBG(8, ("removed accel id=0x%x", tgt->id));
	free(tgt);
}

int xembed_process_kbd_event(XKeyEvent xkey)
{
	struct XEMBEDAccel *tmp;
	int hits = 0;
	for (tmp = tray_data.xembed_data.accels; tmp != NULL; tmp = tmp->next)
		if (tmp->symb == xkey.keycode && tmp->mods == xkey.state) {
			xembed_activate_accelerator(tray_data.dpy, tmp->owner->w,
					tray_data.xembed_data.timestamp, tmp->id, 
					tmp->overloaded ? XEMBED_ACCELERATOR_OVERLOADED : 0);
			hits |= 1;
		}
	return hits;
}

/* FIXME: these functions do not take
 * visual layout into account */
struct TrayIcon *xembed_next_icon()
{
	struct TrayIcon *tmp, *blocker;
	tmp = tray_data.xembed_data.current != NULL ? tray_data.xembed_data.current : NULL;
	blocker = tmp != NULL ? tmp : next_icon(NULL);
	do {
		tmp = next_icon(tmp);
	} while ((!tmp->xembed_supported || !tmp->xembed_wants_focus) && tmp != blocker);
	return tmp;
}

struct TrayIcon *xembed_prev_icon()
{
	struct TrayIcon *tmp, *blocker;
	tmp = tray_data.xembed_data.current != NULL ? tray_data.xembed_data.current : NULL;
	blocker = tmp != NULL ? tmp : prev_icon(tmp);
	do {
		tmp = prev_icon(tmp);
	} while ((!tmp->xembed_supported || !tmp->xembed_wants_focus) && tmp != blocker);
	return tmp;
}


/*********** low level stuff *************/

int xembed_send_msg(Display *dpy, Window dst, long timestamp, long msg, long detail, long data1, long data2)
{
	int status;
	static Atom xa_xembed = None;
	trap_errors();
	
	if (xa_xembed == None) {
		xa_xembed = XInternAtom(dpy, "_XEMBED", False);
	}
	
	status = send_client_msg32(dpy, dst, dst, xa_xembed, timestamp, msg, detail, data1, data2);
	
	if (untrap_errors(dpy)) {
		return trapped_error_code;
	}

	return status;
}

unsigned long xembed_get_info(Display *dpy, Window src, unsigned long *version)
{
	Atom xa_xembed_info, act_type;
	int act_fmt;
	unsigned long nitems, bytesafter;
	unsigned char *data;

	xa_xembed_info = XInternAtom(dpy, "_XEMBED_INFO", False);

	trap_errors();
	
	XGetWindowProperty(dpy, src, xa_xembed_info, 0, 2, False, xa_xembed_info,
			&act_type, &act_fmt, &nitems, &bytesafter, &data);

	if (untrap_errors(dpy)) 
		return XEMBED_ERROR;

	if (act_type != xa_xembed_info) {
		if (version != NULL)
			*version = 0;
		return 0;
	}

	if (nitems < 2) 
		return XEMBED_ERROR;

	if (version != NULL)
			*version = ((unsigned long *) data )[0];

	XFree(data);
	
	return ((unsigned long *) data )[1];
}

int xembed_embedded_notify(Display *dpy, Window src, Window dst, long timestamp)
{
	return xembed_send_msg(dpy, dst, timestamp, XEMBED_EMBEDDED_NOTIFY, 0, src, 0);
}

int xembed_window_activate(Display *dpy, Window dst, long timestamp)
{
	return xembed_send_msg(dpy, dst, timestamp, XEMBED_WINDOW_ACTIVATE, 0, 0, 0);
}

int xembed_window_deactivate(Display *dpy, Window dst, long timestamp)
{
	return xembed_send_msg(dpy, dst, timestamp, XEMBED_WINDOW_DEACTIVATE, 0, 0, 0);
}

int xembed_focus_in(Display *dpy, Window dst, long focus, long timestamp)
{
	DBG(5, ("XEMBED focus set to 0x%x\n", dst));
	return xembed_send_msg(dpy, dst, timestamp, XEMBED_FOCUS_IN, focus, 0, 0);
}

int xembed_focus_out(Display *dpy, Window dst, long timestamp)
{
	return xembed_send_msg(dpy, dst, timestamp, XEMBED_FOCUS_OUT, 0, 0, 0);
}

int xembed_activate_accelerator(Display *dpy, Window dst, long timestamp, long id, long overloaded)
{
	return xembed_send_msg(dpy, dst, timestamp, XEMBED_ACTIVATE_ACCELERATOR, id, overloaded, 0);
}
