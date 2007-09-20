/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* xembed.c
* Tue, 24 Aug 2004 12:05:38 +0700
* -------------------------------
* XEMBED protocol implementation
* -------------------------------*/
#include <X11/Xlib.h>

#include "common.h"
#include "debug.h"
#include "xembed.h"
#include "xutils.h"

int send_xembed_msg(Display *dpy, Window dst, long msg, long detail, long data1, long data2)
{
	int status;
	static Atom xa_xembed = None;
	trap_errors();
	
	if (xa_xembed == None) {
		xa_xembed = XInternAtom(dpy, "_XEMBED", False);
	}
	
	status = send_client_msg32(dpy, dst, dst, xa_xembed, CurrentTime, msg, detail, data1, data2);
	
	if (untrap_errors(dpy)) {
		return trapped_error_code;
	}

	return status;
}

unsigned long xembed_get_info(Display *dpy, Window src, long *version)
{
	Atom xa_xembed_info, act_type;
	int act_fmt;
	unsigned long nitems, bytesafter;
	unsigned char *data;

	xa_xembed_info = XInternAtom(dpy, "_XEMBED_INFO", False);

	trap_errors();
	
	XGetWindowProperty(dpy, src, xa_xembed_info, 0, 2, False, xa_xembed_info,
			&act_type, &act_fmt, &nitems, &bytesafter, &data);

	if (untrap_errors(dpy)) /* FIXME: must somehow indicate a error */
		return 0;

	if (act_type != xa_xembed_info) {
		if (version != NULL)
			*version = 0;
		return 0;
	}

	if (nitems < 2) /* FIXME: the same */
		return 0;

	if (version != NULL)
			*version = ((unsigned long *) data )[0];

	XFree(data);
	
	return ((unsigned long *) data )[1];
}

int xembed_embedded_notify(Display *dpy, Window src, Window dst)
{
	return send_xembed_msg(dpy, dst, XEMBED_EMBEDDED_NOTIFY, 0, src, 0);
}

int xembed_window_activate(Display *dpy, Window dst)
{
	return send_xembed_msg(dpy, dst, XEMBED_WINDOW_ACTIVATE, 0, 0, 0);
}

int xembed_window_deactivate(Display *dpy, Window dst)
{
	return send_xembed_msg(dpy, dst, XEMBED_WINDOW_DEACTIVATE, 0, 0, 0);
}

int xembed_focus_in(Display *dpy, Window dst, int focus)
{
	return send_xembed_msg(dpy, dst, XEMBED_FOCUS_IN, focus, 0, 0);
}

int xembed_focus_out(Display *dpy, Window dst)
{
	return send_xembed_msg(dpy, dst, XEMBED_FOCUS_OUT, 0, 0, 0);
}


