/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* xembed.c
* Tue, 24 Aug 2004 12:05:38 +0700
* -------------------------------
* XEMBED protocol implementation
* -------------------------------*/

/* BIG FAT FIXME: currently tab order is
 * not very dependant on the visual order of 
 * icons. This MUST be fixed (in 0.6) */

/* ANOTHER FAT FIXME: trap_errors()!!! CHECK THE CODE */

#include <X11/X.h>
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

unsigned long xembed_get_info(Display *dpy, Window dst, unsigned long data[2]);
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

void xembed_track_focus_change(int activate);
void xembed_message(XClientMessageEvent ev);

void xembed_init()
{
	/* 1. Initialize data structures */
	tray_data.xembed_data.window_has_focus = False;
	tray_data.xembed_data.current = NULL;
	tray_data.xembed_data.accels = NULL;
	tray_data.xembed_data.timestamp = CurrentTime;
	tray_data.xembed_data.xa_xembed = XInternAtom(tray_data.dpy, "_XEMBED", False);
	tray_data.xembed_data.xa_xembed_info = XInternAtom(tray_data.dpy, "_XEMBED_INFO", False);
	/* 2. Create focus proxy (see XEMBED spec) */
	tray_data.xembed_data.focus_proxy = 
		XCreateSimpleWindow(tray_data.dpy, tray_data.tray, -1, -1, 1, 1, 0, 0, 0);
	XSelectInput(tray_data.dpy, tray_data.xembed_data.focus_proxy, FocusChangeMask);
	XMapRaised(tray_data.dpy, tray_data.xembed_data.focus_proxy);
	DBG(8, ("created focus proxy, wid=0x%x\n", tray_data.xembed_data.focus_proxy));
}

void xembed_handle_event(XEvent ev)
{
	switch (ev.type) {
	case FocusOut:
		DBG(8, ("FocusOut 0x%x\n", ev.xfocus.window));
		if (ev.xfocus.window == tray_data.xembed_data.focus_proxy)
			xembed_track_focus_change(False);
		break;
	case FocusIn:
		DBG(8, ("FocusIn 0x%x\n", ev.xfocus.window));
		if (ev.xfocus.window == tray_data.tray) {
			tray_data.xembed_data.timestamp = get_server_timestamp(tray_data.dpy, tray_data.tray);
			trap_errors();
			DBG(9, ("XSetInputFocus(\n"));
			DBG(9, ("  dpy = %p,\n", tray_data.dpy));
			DBG(9, ("  window = 0x%x,\n", tray_data.xembed_data.focus_proxy));
			DBG(9, ("  revert = 0x%x,\n", RevertToParent));
			DBG(9, ("  timestamp = %d\n", tray_data.xembed_data.timestamp));
			DBG(9, (")\n"));
			XSetInputFocus(tray_data.dpy, tray_data.xembed_data.focus_proxy, RevertToParent, tray_data.xembed_data.timestamp);
			if (untrap_errors(tray_data.dpy)) {
#ifdef DEBUG
				DBG(0, ("XSetInputFocus failed on focus proxy\n"));
				{
					XWindowAttributes xwa;
					trap_errors();
					XGetWindowAttributes(tray_data.dpy, tray_data.xembed_data.focus_proxy, &xwa);
					if (untrap_errors(tray_data.dpy)) {
						DBG(0, ("  proxy window is gone\n"));
					} else {
						DBG(0, ("  proxy window is here\n"));
						DBG(0, ("  map state = %s\n", xwa.map_state == IsUnmapped ? "IsUnmapped" : (xwa.map_state == IsUnviewable ? "IsUnviewable" : "IsViewable")));
						XGetWindowAttributes(tray_data.dpy, tray_data.tray, &xwa);
						DBG(0, ("  tray map state = %s\n", xwa.map_state == IsUnmapped ? "IsUnmapped" : (xwa.map_state == IsUnviewable ? "IsUnviewable" : "IsViewable")));
						DBG(0, ("  retrying\n"));
						trap_errors();
						XSetInputFocus(tray_data.dpy, tray_data.xembed_data.focus_proxy, RevertToParent, tray_data.xembed_data.timestamp);
						if (untrap_errors(tray_data.dpy))
							DBG(0, ("    failed\n"));
						else
							DBG(0, ("    ok\n"));
					}
				}
				DIE(("could not set input focus to proxy window\n"));
#else
				DIE(("internal error.\n"
					 "please consider building a debug version if the problem persists\n"
					 "and send a mail to the author (see AUTHORS file)\n"));
#endif
			} else
				DBG(8, ("Focus set to focus proxy\n"));
			xembed_track_focus_change(True);
		}
		break;
	case ClientMessage:
		if (ev.xclient.message_type == tray_data.xembed_data.xa_xembed) {
			xembed_message(ev.xclient);
#if 0
		} else if (ev.xclient.message_type == tray_data.xa_wm_protocols &&
				   ev.xclient.data.l[0] == tray_data.xa_wm_take_focus) 
		{
#endif
		} else if (ev.xclient.message_type == tray_data.xa_tray_opcode) {
			/* we peek at _NET_SYSTEM_TRAY_OPCODE messages 
			 * to obtain proper timestamp for embedding */
			tray_data.xembed_data.timestamp = ev.xclient.data.l[0];
			if (tray_data.xembed_data.timestamp == CurrentTime) 
				tray_data.xembed_data.timestamp = get_server_timestamp(tray_data.dpy, tray_data.tray);
		}
		break;
	case KeyRelease:
	case KeyPress:
		tray_data.xembed_data.timestamp = ev.xkey.time;
		if (ev.type == KeyRelease && xembed_process_kbd_event(ev.xkey))
			break;
		if (tray_data.xembed_data.current != NULL) {
			DBG(9, ("current wants focus: %d\n", tray_data.xembed_data.current->is_xembed_wants_focus));
			trap_errors();
			XSendEvent(tray_data.dpy, tray_data.xembed_data.current->wid, False, NoEventMask, &ev);
			if (untrap_errors(tray_data.dpy)) {
				tray_data.xembed_data.current->is_invalid = True;
				return;
			}
			DBG(9, ("Sent key event to 0x%x\n", tray_data.xembed_data.current->wid));
		}
		break;
	}
}

int xembed_check_support(struct TrayIcon *ti)
{
	unsigned long xembed_data[2];
	ti->is_xembed_supported = xembed_get_info(tray_data.dpy, ti->wid, xembed_data);
	ti->is_hidden = !xembed_is_mapped(ti);
	return !ti->is_invalid;
}

int xembed_embed(struct TrayIcon *ti)
{
	if (!ti->is_xembed_supported) return SUCCESS;
	
	DBG(8, ("timestamp = %u\n", tray_data.xembed_data.timestamp));

	ti->is_xembed_wants_focus = True;

	if (!xembed_embedded_notify(tray_data.dpy, tray_data.tray, ti->wid, tray_data.xembed_data.timestamp))
		return FAILURE;

	ti->xembed_last_timestamp = tray_data.xembed_data.timestamp;
	ti->xembed_last_msgid = XEMBED_EMBEDDED_NOTIFY;

	if (tray_data.xembed_data.current == NULL) {
		if (!xembed_focus_in(tray_data.dpy, ti->wid, XEMBED_FOCUS_FIRST, tray_data.xembed_data.timestamp))
			return FAILURE;
		tray_data.xembed_data.current = ti;
	}
	
	if (tray_data.xembed_data.window_has_focus) 
		return xembed_window_activate(tray_data.dpy, ti->wid, tray_data.xembed_data.timestamp);

	return SUCCESS;
}

int xembed_unembed(struct TrayIcon *ti)
{
	struct TrayIcon *tmp;
	tray_data.xembed_data.timestamp = get_server_timestamp(tray_data.dpy, tray_data.tray);
	if (ti == tray_data.xembed_data.current) {
		tmp = xembed_next_icon(ti);
		if (tmp == ti || tmp->is_xembed_wants_focus == False) {
			xembed_switch_focus_to(NULL, 0);
		} else {
			xembed_switch_focus_to(tmp, XEMBED_FOCUS_FIRST);
		}
	}
	return SUCCESS;
}

int xembed_is_mapped(struct TrayIcon *ti)
{
	/* if _XMEBED_INFO is not present, returns True */
	unsigned long xembed_data[2];
	int rc;
	rc = xembed_get_info(tray_data.dpy, ti->wid, xembed_data);
	if (rc) {
		return ((xembed_data[1] & XEMBED_MAPPED) != 0);
	} else if (ti->is_xembed_supported && !rc) {
		ti->is_invalid = True;
		return False;
	} else
		return True;
}

int xembed_hide(struct TrayIcon *ti)
{
	unsigned long xembed_data[2];
	if (xembed_get_info(tray_data.dpy, ti->wid, xembed_data)){
		xembed_data[2] &= ~XEMBED_MAPPED;
		trap_errors();
		XChangeProperty(tray_data.dpy, ti->wid,
				tray_data.xembed_data.xa_xembed_info,
				tray_data.xembed_data.xa_xembed_info,
				32, PropModeReplace, (unsigned char *) xembed_data, 2);
		if (untrap_errors(tray_data.dpy)) {
			ti->is_invalid = True;
			return FAILURE;
		}
	}
	return FAILURE;
}

/*********** implementation level ***************/
void xembed_switch_focus_to(struct TrayIcon *tgt, long focus)
{
	DBG(5, ("tgt=%p, focus=%d\n", tgt, focus));
	if (tray_data.xembed_data.current != NULL) {
		DBG(5, ("focus removed from 0x%x (pointer %p)\n", tray_data.xembed_data.current->wid, tray_data.xembed_data.current));
		xembed_focus_out(tray_data.dpy, tray_data.xembed_data.current->wid, tray_data.xembed_data.timestamp);
	}

	if (tgt != NULL) {
		xembed_focus_in(tray_data.dpy, tgt->wid, focus, tray_data.xembed_data.timestamp);
		DBG(5, ("focus set to 0x%x (pointer %p)\n", tgt->wid, tgt));
	} else
		DBG(5, ("focus set no none\n"));

	tray_data.xembed_data.current = tgt;
}

static int activate = 0;

int broadcast_activate_msg(struct TrayIcon *ti)
{
	if (activate)
		xembed_window_activate(tray_data.dpy, ti->wid, tray_data.xembed_data.timestamp);
	else
		xembed_window_deactivate(tray_data.dpy, ti->wid, tray_data.xembed_data.timestamp);

	return NO_MATCH;
}

void xembed_track_focus_change(int in)
{
	if (tray_data.xembed_data.window_has_focus == in) return;
	tray_data.xembed_data.window_has_focus = in;
	activate = in;
	forall_icons(&broadcast_activate_msg);
	DBG(8, ("XEMBED focus: %s\n", in ? "ON" : "OFF"));
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
		DBG(8, ("xembed_data.current = %p (window: 0x%x)\n", tray_data.xembed_data.current, tray_data.xembed_data.current->wid));
	else
		DBG(8, ("XEMBED focus is unset\n"));
#endif

	if ((ti = find_icon_ex(ev.window)) == NULL) return;
	if (!ti->is_xembed_supported) {
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
		DBG(5, ("new focus target: 0x%x (pointer %p)\n", tgt->wid, tgt));
		ti->is_xembed_wants_focus = True;
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
			DBG(5, ("new focus target: 0x%x (pointer %p)\n", tgt->wid, tgt));
			if (tgt->is_xembed_wants_focus == False) 
			{
				xembed_switch_focus_to(NULL, 0);
				break;
			}
			else if (tgt->xembed_last_timestamp == tray_data.xembed_data.timestamp &&
			         (tgt->xembed_last_msgid == XEMBED_FOCUS_NEXT ||
			          tgt->xembed_last_msgid == XEMBED_FOCUS_PREV))
			{
				DBG(5, ("focus disabled for 0x%x (pointer %p)\n", tgt->wid, tgt));
				tgt->is_xembed_wants_focus = False;
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

void xembed_add_accel(struct TrayIcon* owner, long id, long symb, long mods)
{
	struct XEMBEDAccel *xaccel, *tmp;
	
	xaccel = (struct XEMBEDAccel *) malloc(sizeof(struct XEMBEDAccel));
	if (xaccel == NULL) 
		DIE(("Out of memory while registering XEMBED accelerator\n"));

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
			xembed_activate_accelerator(tray_data.dpy, tmp->owner->wid,
					tray_data.xembed_data.timestamp, tmp->id, 
					tmp->overloaded ? XEMBED_ACCELERATOR_OVERLOADED : 0);
			hits |= 1;
		}
	return hits;
}

/* FIXME (for 0.6): these functions do not take visual layout into account */
struct TrayIcon *xembed_next_icon()
{
	struct TrayIcon *tmp, *blocker;
	tmp = tray_data.xembed_data.current != NULL ? tray_data.xembed_data.current : NULL;
	blocker = tmp != NULL ? tmp : next_icon(NULL);
	do {
		tmp = next_icon(tmp);
	} while ((!tmp->is_xembed_supported || !tmp->is_xembed_wants_focus) && tmp != blocker);
	return tmp;
}

struct TrayIcon *xembed_prev_icon()
{
	struct TrayIcon *tmp, *blocker;
	tmp = tray_data.xembed_data.current != NULL ? tray_data.xembed_data.current : NULL;
	blocker = tmp != NULL ? tmp : prev_icon(tmp);
	do {
		tmp = prev_icon(tmp);
	} while ((!tmp->is_xembed_supported || !tmp->is_xembed_wants_focus) && tmp != blocker);
	return tmp;
}

/*********** low level stuff *************/
int xembed_send_msg(Display *dpy, Window dst, long timestamp, long msg, long detail, long data1, long data2)
{
	int status;
	trap_errors();
	
	status = send_client_msg32(dpy, dst, dst, tray_data.xembed_data.xa_xembed, timestamp, msg, detail, data1, data2);
	
	if (untrap_errors(dpy)) {
		return trapped_error_code;
	}

	return status;
}

unsigned long xembed_get_info(Display *dpy, Window src, unsigned long data[2])
{
	Atom act_type;
	int act_fmt;
	unsigned long nitems, bytesafter;
	unsigned char *tmpdata;

	data[0] = 0;
	data[1] = 0;

	trap_errors();
	
	XGetWindowProperty(dpy, src, tray_data.xembed_data.xa_xembed_info, 
			0, 2, False, tray_data.xembed_data.xa_xembed_info, 
			&act_type, &act_fmt, &nitems, &bytesafter, &tmpdata);

	if (untrap_errors(dpy))
		return FAILURE;

	if (act_type != tray_data.xembed_data.xa_xembed_info || nitems < 2)
		return FAILURE;

	if (tmpdata != NULL) {
			data[0] = ((unsigned long *) tmpdata)[0];
			data[1] = ((unsigned long *) tmpdata)[1];
	}

	XFree(tmpdata);
	
	return SUCCESS;
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
