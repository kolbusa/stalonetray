/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * main.c
 * Tue, 24 Aug 2004 12:00:24 +0700
 * -------------------------------
 * main is main
 * ------------------------------- */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xmd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* from System Tray Protocol Specification
 * http://freedesktop.org/Standards/systemtray-spec/systemtray-spec-0.1.html */
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define	TRAY_SEL_ATOM "_NET_SYSTEM_TRAY_S"

#include "common.h"
#include "xutils.h"
#include "xembed.h"

/* just globals */
Display		*dpy;

Window		wnd;
Window		buttons[4];

CARD32		xembed_data[2];

int 		has_focus = 0;
int			sel_idx = 0;
int			embedded = 0;
int			do_not_accept_focus = 0;
int			request_focus = 0;
int			register_accel = 0;

CARD32		accel_id = 0x11ff11ff;
CARD32		accel_sym = XK_Page_Down;
CARD32		accel_mods = 0x14; /* Ctrl */

Window 		tray = None;

CARD32		current_xembed_timestamp;

XColor		wnd_bg_embedded = {.pixel = 0x000000};
XColor		wnd_bg_unembedded = {.pixel = 0x770000};
XColor		buttons_focused = {.pixel = 0x999999};
XColor		buttons_unfocused = {.pixel = 0x666666};
XColor		buttons_selected = {.pixel = 0x0000ff};

XSizeHints	xsh = {
	flags:	(PSize | PPosition),
	x:		100,
	y:		100,
	width:	24,
	height:	24
};

Atom 		xa_xembed_info;
Atom 		xa_tray_selection;
Atom		xa_tray_opcode;
Atom		xa_tray_data;
Atom 		xa_wm_delete_window;
Atom		xa_wm_protocols;
Atom		xa_wm_take_focus;
Atom 		xa_xembed;

void create_window(int argc, char **argv)
{
#if 0
	XWMHints			xwmh = {
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
#else
	XWMHints			*xwmh;
#endif
	char				*wnd_name = "test_tray_icon";
	XTextProperty		wm_name;
	XClassHint			*xch;
	XWindowAttributes 	xwa;
	int 				x, y;

	xch = XAllocClassHint();
	xch->res_class = wnd_name;
	xch->res_name = wnd_name;

	wnd = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
					xsh.x, xsh.y, xsh.width, xsh.height, 0, 0, wnd_bg_unembedded.pixel);

	printf("Created window: 0x%x\n", wnd);

	for (x = 0; x < 2; x++)
		for (y = 0; y < 2; y++) {
			buttons[x + y * 2] = XCreateSimpleWindow(dpy, wnd, 2 + x * 12, 2 + y * 12, 8, 8, 0, 0, buttons_unfocused.pixel);
			XMapRaised(dpy, buttons[x + y * 2]);
		}

	if (!x11_ok()) {
		DIE(("Error: could not create simple window\n"));
	}

	xembed_data[0] = 0;
	xembed_data[1] = 1;

	xembed_post_data(dpy, wnd, xembed_data);

	XmbTextListToTextProperty(dpy, &wnd_name, 1, XTextStyle, &wm_name);

	xwmh = XAllocWMHints();
	xwmh->flags = InputHint | StateHint | WindowGroupHint | IconWindowHint;
	xwmh->flags = True;
	xwmh->initial_state = NormalState;
	xwmh->window_group = 0;
	xwmh->icon_window = None;

	XSetWMProperties(dpy, wnd, &wm_name, NULL, argv, argc, &xsh, xwmh, xch);

	if ((tray = XGetSelectionOwner(dpy, xa_tray_selection)) == None) {
		printf("Error: no tray found\n");
	} else {
		x11_send_client_msg32(dpy, tray, wnd, xa_tray_opcode, CurrentTime, SYSTEM_TRAY_REQUEST_DOCK, wnd, 0, 0);
	}

	XSelectInput(dpy, wnd, SubstructureNotifyMask | StructureNotifyMask | PropertyChangeMask | KeyReleaseMask | ButtonReleaseMask );
	
	XMapRaised(dpy, wnd);	

}

void redraw_buttons()
{
	int i;
	unsigned long pixel = 0;
	for (i = 0; i < 4; i++) {
		if (has_focus) {
			if (i == sel_idx)
				pixel = buttons_selected.pixel;
			else
				pixel = buttons_focused.pixel;
		} else
			pixel = buttons_unfocused.pixel;

		XSetWindowBackground(dpy, buttons[i], pixel);
		XClearWindow(dpy, buttons[i]);
	}
}

void focus_in(int how)
{
	DBG(0, ("got focus\n"));
	if (do_not_accept_focus) {
		if (how == XEMBED_FOCUS_FIRST)
			xembed_send_focus_next(dpy, tray, current_xembed_timestamp);
		else
			xembed_send_focus_prev(dpy, tray, current_xembed_timestamp);
	} else {
		has_focus = 1;
		if (how == XEMBED_FOCUS_FIRST)
			sel_idx = 0;
		else if (how == XEMBED_FOCUS_LAST)
			sel_idx = 3;
		redraw_buttons();
	}
}

void focus_out()
{
	DBG(0, ("lost focus\n"));
	has_focus = 0;
	redraw_buttons();
}

void focus_next()
{
	if (!embedded) {
		sel_idx = (sel_idx + 1) % 4;
		redraw_buttons();
	} else {
		if (sel_idx < 3) {
			sel_idx++;
			redraw_buttons();
		} else
			xembed_send_focus_next(dpy, tray, x11_get_server_timestamp(dpy, wnd));
	}
}

void focus_prev()
{
	DBG(0, ("focus_prev\n"));
	if (!embedded) {
		sel_idx = (4 + (sel_idx - 1)) % 4;
		redraw_buttons();
	} else {
		if (sel_idx > 0) {
			sel_idx--;
			redraw_buttons();
		} else
			xembed_send_focus_prev(dpy, tray, x11_get_server_timestamp(dpy, wnd));
	};
	DBG(0, ("sel_idx=%d\n", sel_idx));
}

void client_event(XClientMessageEvent ev)
{
    char *msg_type_name;

	msg_type_name = XGetAtomName(dpy, ev.message_type);

	if (msg_type_name != NULL) {
        DBG(3, ("message name: \"%s\"\n", msg_type_name));
        XFree(msg_type_name);
    }

	if (ev.message_type == xa_xembed) {
		DBG(3, ("XEMBED message opcode: %d\n", ev.data.l[1]));
		
		current_xembed_timestamp = ev.data.l[0];
		if (current_xembed_timestamp == CurrentTime)
			current_xembed_timestamp = x11_get_server_timestamp(dpy, wnd);

		switch (ev.data.l[1]) {
		case XEMBED_EMBEDDED_NOTIFY:
			embedded = 1;
			XSetWindowBackground(dpy, wnd, wnd_bg_embedded.pixel);
			XClearWindow(dpy, wnd);
			redraw_buttons();
			if (request_focus)
				xembed_send_request_focus(dpy, tray, x11_get_server_timestamp(dpy, wnd));
			break;
		case XEMBED_FOCUS_OUT:
			focus_out();
			break;
		case XEMBED_FOCUS_IN:
			focus_in(ev.data.l[2]);
			break;
		case XEMBED_ACTIVATE_ACCELERATOR:
			DBG(3, ("Got ACTIVATE_ACCELERATOR message, id=0x%x, overloaded=0x%x", ev.data.l[2], ev.data.l[3]));
			break;
		}
			
	}
}

void key_release(XKeyReleasedEvent ev)
{
	char buf[20];
	KeySym keysym;

	XLookupString(&ev, buf, 20, &keysym, NULL);

	DBG(0, ("key_release: code=%d, mask=0x%x, sym=0x%x\n", ev.keycode, ev.state, keysym));

	switch (keysym) {
	case XK_Tab:
		focus_next();
		break;
	case XK_ISO_Left_Tab:
		focus_prev();
		break;
	default:
		break;
	}
}

int main(int argc, char** argv)
{
	XEvent		ev;
	
	char		*tray_sel_atom_name;
	char		*dpy_name = NULL;

	int			i;

	for (i = 1; i < argc; i++)
		if (!strcmp(argv[i], "-n"))
			do_not_accept_focus = 1;
		else if (!strcmp(argv[i], "-r"))
			request_focus = 1;
		else if (!strcmp(argv[i], "-a"))
			register_accel = 1;

	if ((dpy = XOpenDisplay(dpy_name)) == NULL) {
		DIE(("Error: could not open display\n"));
	}

	x11_trap_errors();	
	
	if ((tray_sel_atom_name = (char *)malloc(strlen(TRAY_SEL_ATOM) + 2)) == NULL) {
		DIE(("OOM\n"));
	}

	snprintf(tray_sel_atom_name, strlen(TRAY_SEL_ATOM) + 2,
		"%s%u", TRAY_SEL_ATOM, DefaultScreen(dpy));

	xa_tray_selection = XInternAtom(dpy, tray_sel_atom_name, False);

	free(tray_sel_atom_name);
	
	xa_tray_opcode = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	xa_tray_data = XInternAtom(dpy, "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);
	xa_xembed_info = XInternAtom(dpy, "_XEMBED_INFO", False);
	xa_xembed = XInternAtom(dpy, "_XEMBED", False);

	create_window(argc, argv);

	if (register_accel)
		xembed_send_register_accelerator(dpy, tray, x11_get_server_timestamp(dpy, wnd), accel_id, accel_sym, accel_mods);

	XFlush(dpy);

	redraw_buttons();

	for(;;) {
		XNextEvent(dpy, &ev);
		switch(ev.type) {
		case ClientMessage:
			client_event(ev.xclient);
			break;
/*        case DestroyNotify:*/
/*            return 0;*/
		case ConfigureNotify:
			/* Maintain size */
			if (ev.xconfigure.width != xsh.width || ev.xconfigure.height != xsh.height)
				XResizeWindow(dpy, wnd, xsh.width, xsh.height);
			break;
		case KeyRelease:
			key_release(ev.xkey);
			break;
		case ButtonRelease:
			xembed_send_request_focus(dpy, tray, x11_get_server_timestamp(dpy, wnd));
			break;
		case UnmapNotify:
			if (ev.xunmap.window == wnd) XMapRaised(dpy, wnd);
		default:
			break;
		}
	}
	
	return 0;
}
