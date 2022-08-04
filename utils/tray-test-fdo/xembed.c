/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * icons.h
 * Tue, 24 Aug 2004 12:05:38 +0700
 * -------------------------------
 * XEMBED protocol implementation
 * -------------------------------*/

#include "xembed.h"

/* error handling code taken from xembed spec */
int trapped_error_code = 0;
static int (*old_error_handler)(Display *, XErrorEvent *);

static int error_handler(Display *display, XErrorEvent *error)
{
    trapped_error_code = error->error_code;
    return 0;
}

void trap_errors()
{
    trapped_error_code = 0;
    old_error_handler = XSetErrorHandler(error_handler);
}

int untrap_errors()
{
    XSetErrorHandler(old_error_handler);
    return trapped_error_code;
}

int xclient_msg32(Display *dpy, Window dst, Atom type, long data0, long data1,
    long data2, long data3, long data4)
{
    XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.window = dst;
    ev.xclient.message_type = type;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = data0;
    ev.xclient.data.l[1] = data1;
    ev.xclient.data.l[2] = data2;
    ev.xclient.data.l[3] = data3;
    ev.xclient.data.l[4] = data4;

    return XSendEvent(dpy, dst, False, NoEventMask, &ev);
}

int xembed_msg(
    Display *dpy, Window dst, long msg, long detail, long data1, long data2)
{
    int status;
    static Atom xa_xembed = None;
    trap_errors();
    if (xa_xembed == None) xa_xembed = XInternAtom(dpy, "_XEMBED", False);
    status = xclient_msg32(
        dpy, dst, xa_xembed, CurrentTime, msg, detail, data1, data2);
    if (untrap_errors()) return trapped_error_code;
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

    if (untrap_errors()) /* FIXME: must somehow indicate a error */
        return 0;

    if (act_type != xa_xembed_info) {
        if (version != NULL) *version = 0;
        return 0;
    }

    if (nitems < 2) /* FIXME: the same */
        return 0;

    if (version != NULL) *version = ((unsigned long *)data)[0];

    XFree(data);

    return ((unsigned long *)data)[1];
}

int xembed_embeded_notify(Display *dpy, Window src, Window dst)
{
    return xembed_msg(dpy, dst, XEMBED_EMBEDDED_NOTIFY, 0, src, 0);
}

int xembed_window_activate(Display *dpy, Window dst)
{
    return xembed_msg(dpy, dst, XEMBED_WINDOW_ACTIVATE, 0, 0, 0);
}

int xembed_window_deactivate(Display *dpy, Window dst)
{
    return xembed_msg(dpy, dst, XEMBED_WINDOW_DEACTIVATE, 0, 0, 0);
}

int xembed_focus_in(Display *dpy, Window dst, int focus)
{
    return xembed_msg(dpy, dst, XEMBED_FOCUS_IN, focus, 0, 0);
}

int xembed_focus_out(Display *dpy, Window dst)
{
    return xembed_msg(dpy, dst, XEMBED_FOCUS_OUT, 0, 0, 0);
}
