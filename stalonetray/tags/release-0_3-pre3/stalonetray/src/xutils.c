/* ************************************
 * vim:tabstop=4:shiftwidth=4
 * xutils.c
 * Sun, 05 Mar 2006 17:56:56 +0600
 * ************************************
 * X11 utilities
 * ************************************/

#include "config.h"

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "xembed.h"
#include "debug.h"

/* error handling code taken from xembed spec */
int trapped_error_code = 0;
static int (*old_error_handler) (Display *, XErrorEvent *);

 
static int error_handler(Display *dpy, XErrorEvent *error)
{
	static char buffer[1024];
#ifdef DEBUG
	XGetErrorText(dpy, error->error_code, buffer, sizeof(buffer) - 1);
	DBG(0, "Error: %s\n", buffer);
#endif
	trapped_error_code = error->error_code;
	return 0;
}

void trap_errors()
{
	trapped_error_code = 0;
	old_error_handler = XSetErrorHandler(error_handler);
	DBG(8, "old_error_handler = 0x%x\n", old_error_handler);
}

int untrap_errors(Display *dpy)
{
#ifdef DEBUG
	/* in order to catch up with errors in async mode 
	 * XXX: this slows things down...*/
	XSync(dpy, False);
#endif
	XSetErrorHandler(old_error_handler);
	return trapped_error_code;
}

int send_client_msg32(Display *dpy, Window dst, Atom type, long data0, long data1, long data2, long data3, long data4)
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

int set_window_size(struct TrayIcon *ti, struct Point dim)
{
	XSizeHints xsh = {
		.flags = PSize,
		.width = dim.x,
		.height = dim.y
	};

	trap_errors();
	
	XSetWMNormalHints(tray_data.dpy, ti->w, &xsh);

	XResizeWindow(tray_data.dpy, ti->w, dim.x, dim.y);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, "failed to force 0x%x size to (%d, %d) (error code %d)\n", 
				ti->w, dim.x, dim.y, trapped_error_code);
		ti->l.wnd_sz.x = 0;
		ti->l.wnd_sz.y = 0;
		return FAILURE;
	}
	ti->l.wnd_sz = dim;
	return SUCCESS;
}

#define NUM_GETSIZE_RETRIES	100
int get_window_size(struct TrayIcon *ti, struct Point *res)
{
	XWindowAttributes xwa;
	int atmpt = 0;

	res->x = FALLBACK_SIZE;
	res->y = FALLBACK_SIZE;
	
	for(;;) {
		trap_errors();

		XGetWindowAttributes(tray_data.dpy, ti->w, &xwa);
	
		if (untrap_errors(tray_data.dpy)) {
			DBG(0, "failed to get 0x%x attributes (error code %d)\n", ti->w, trapped_error_code);
			return FAILURE;
		}
	
		res->x = xwa.width;
		res->y = xwa.height;

		if (res->x * res->y > 1) {
			DBG(4, "success (%dx%d)\n", res->x, res->y);
			return SUCCESS;
		}

		if (atmpt++ > NUM_GETSIZE_RETRIES) {
			DBG(0, "timeout waiting for 0x%x to specify its size. Using fallback (%d,%d)\n", 
					ti->w, FALLBACK_SIZE, FALLBACK_SIZE);
			return SUCCESS;
		}
		usleep(500);
	};
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

