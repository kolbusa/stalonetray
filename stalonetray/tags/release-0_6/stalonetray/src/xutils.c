/* ************************************
 * vim:tabstop=4:shiftwidth=4
 * xutils.c
 * Sun, 05 Mar 2006 17:56:56 +0600
 * ************************************
 * misc X11 utilities
 * ************************************/

#include "config.h"

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <limits.h>
#include <unistd.h>

#include "xutils.h"

#include "xembed.h"
#include "debug.h"
#include "common.h"

/* error handling code (modified code taken from xembed spec) */
int trapped_error_code = 0;
#define MAX_TRAP_DEPTH 10
#ifdef DEBUG
char *loc_fname[MAX_TRAP_DEPTH];
char *loc_func[MAX_TRAP_DEPTH];
int  loc_line[MAX_TRAP_DEPTH];
#endif
static int trap_depth = -1;
int (*old_error_handler[MAX_TRAP_DEPTH]) (Display *, XErrorEvent *);

int error_handler(Display *dpy, XErrorEvent *error)
{
#ifdef DEBUG
	static char buffer[1024];
	XGetErrorText(dpy, error->error_code, buffer, sizeof(buffer) - 1);
	DBG(0, ("X11 error: %s\n", buffer));
	DBG(0, ("generated after trap_errors() at %s:%d:%s()\n", 
			loc_fname[trap_depth], loc_line[trap_depth], loc_func[trap_depth]));
#endif
	trapped_error_code = error->error_code;
	return 0;
}

#ifdef DEBUG
void trap_errors_real(char *fname, int line, char *func)
#else
void trap_errors_real()
#endif
{
	if (trap_depth >= MAX_TRAP_DEPTH - 1)
		return;

	trap_depth++;
#ifdef DEBUG
	loc_fname[trap_depth] = fname;
	loc_line[trap_depth] = line;
	loc_func[trap_depth] = func;
#endif
	old_error_handler[trap_depth] = XSetErrorHandler(error_handler);
	trapped_error_code = 0;
	DBG(11, ("old_error_handler = %p\n", old_error_handler));
	DBG(11, ("trap_depth = %d (%s:%d:%s())\n", trap_depth, fname, line, func));
}

int untrap_errors(Display *dpy)
{
	/* in order to catch up with errors in async mode 
	 * XXX: this slows things down...*/
#ifdef DEBUG
#if 0
	XSync(dpy, False);
#endif
#endif
	XSetErrorHandler(old_error_handler[trap_depth]);
	trap_depth--;
	return trapped_error_code;
}

static Window timestamp_wnd;
static Atom timestamp_atom = None;

Bool wait_timestamp(Display *dpy, XEvent *xevent, XPointer data)
{
	Window wnd = *(Window *)data;

	if (xevent->type == PropertyNotify &&
	    xevent->xproperty.window == wnd &&
		xevent->xproperty.atom == timestamp_atom)
	{
		return True;
	}

	return False;
}

Time get_server_timestamp(Display *dpy, Window wnd)
{
	unsigned char c = 's';
	XEvent xevent;

	if (timestamp_atom == None) 
		timestamp_atom = XInternAtom(dpy, "STALONETRAY_TIMESTAMP", False);

	trap_errors();

	XChangeProperty(dpy, wnd, timestamp_atom, timestamp_atom, 8, PropModeReplace, &c, 1);

	if (untrap_errors(dpy))
		return CurrentTime;

	timestamp_wnd = wnd;
	trap_errors();
	XIfEvent(dpy, &xevent, wait_timestamp, (XPointer)&timestamp_wnd);
	untrap_errors(dpy);

	return xevent.xproperty.time;
}

int get_root_window_prop(Display *dpy, Atom atom, Window **data, unsigned long *len)
{
	Atom act_type;
	Window root_wnd;
	int act_fmt;
	unsigned long bytes_after, prop_len, wnd_list_len;
	Window *wnd_list = NULL;
	
	root_wnd = DefaultRootWindow(dpy);

	*data = NULL; *len = 0;

	trap_errors();

	XGetWindowProperty(dpy, DefaultRootWindow(dpy), atom, 
		    0L, 0L, False, XA_WINDOW, &act_type, &act_fmt,
		&prop_len, &bytes_after, (unsigned char**)&wnd_list);

	prop_len = bytes_after / sizeof(Window);

	XGetWindowProperty(dpy, DefaultRootWindow(dpy), atom,
			0L, prop_len, False, XA_WINDOW, &act_type, &act_fmt,
			&wnd_list_len, &bytes_after, (unsigned char **)&wnd_list);

	if (untrap_errors(dpy)) {
		return FAILURE;
	}

	*len = wnd_list_len;
	*data = wnd_list;
	return SUCCESS;
}

int send_client_msg32(Display *dpy, Window dst, Window wnd, Atom type, long data0, long data1, long data2, long data3, long data4)
{
	XEvent ev;
	ev.xclient.type = ClientMessage;
	ev.xclient.serial = 0;
	ev.xclient.send_event = True;
	ev.xclient.message_type = type;
	ev.xclient.window = wnd;
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = data0;
	ev.xclient.data.l[1] = data1;
	ev.xclient.data.l[2] = data2;
	ev.xclient.data.l[3] = data3;
	ev.xclient.data.l[4] = data4;
/*	return XSendEvent(dpy, dst, False, SubstructureRedirectMask | SubstructureNotifyMask | StructureNotifyMask, &ev);*/
	return XSendEvent(dpy, dst, False, 0xFFFFFF, &ev);
}

int repaint_icon(struct TrayIcon *ti)
{
	XEvent xe;

	trap_errors();
	
	XClearWindow(tray_data.dpy, ti->mid_parent);

	XClearWindow(tray_data.dpy, ti->wid);

	xe.type = VisibilityNotify;

	xe.xvisibility.window = ti->wid;
	xe.xvisibility.state = VisibilityFullyObscured;

	XSendEvent(tray_data.dpy, ti->wid, True, NoEventMask, &xe); 

	xe.xvisibility.state = VisibilityUnobscured;

	XSendEvent(tray_data.dpy, ti->wid, True, NoEventMask, &xe); 

	xe.type = Expose;
	xe.xexpose.window = ti->wid;
	xe.xexpose.x = 0;
	xe.xexpose.y = 0;
	xe.xexpose.width = ti->l.wnd_sz.x;
	xe.xexpose.height = ti->l.wnd_sz.y;
	xe.xexpose.count = 0;

	XClearWindow(tray_data.dpy, ti->wid);
		
	XSendEvent(tray_data.dpy, ti->wid, True, NoEventMask, &xe); 

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("could not ask  0x%x for repaint (%d)\n", ti->wid, trapped_error_code));
		ti->is_invalid = True;
	}

	return NO_MATCH;
}

int set_window_size(struct TrayIcon *ti, struct Point dim)
{
	XSizeHints xsh;
	
	xsh.flags = PSize;
	xsh.width = dim.x;
	xsh.height = dim.y;

	trap_errors();
	
	XSetWMNormalHints(tray_data.dpy, ti->wid, &xsh);

	XResizeWindow(tray_data.dpy, ti->wid, dim.x, dim.y);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("failed to force 0x%x size to (%d, %d) (error code %d)\n", 
				ti->wid, dim.x, dim.y, trapped_error_code));
		ti->is_invalid = True;
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
	
	for (;;) {
		trap_errors();

		XGetWindowAttributes(tray_data.dpy, ti->wid, &xwa);
	
		if (untrap_errors(tray_data.dpy)) {
			DBG(0, ("failed to get 0x%x attributes (error code %d)\n", ti->wid, trapped_error_code));
			return FAILURE;
		}
	
		res->x = xwa.width;
		res->y = xwa.height;
		if (res->x * res->y > 1) {
			DBG(4, ("success (%dx%d)\n", res->x, res->y));
			return SUCCESS;
		}
		if (atmpt++ > NUM_GETSIZE_RETRIES) {
			DBG(0, ("timeout waiting for 0x%x to specify its size, using fallback (%d,%d)\n", 
					ti->wid, FALLBACK_SIZE, FALLBACK_SIZE));
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
		pix = (Pixmap)(*(long *)reteval);
	}
	if (reteval)
	{
		XFree(reteval);
	}
	DBG(8, ("pixmap: 0x%x\n", pix));
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
	xa_rootpix = tray_data.xa_xrootpmap_id != None ? tray_data.xa_xrootpmap_id : xa_xrootpmap_id;
	
	pix = get_root_pixmap(tray_data.xa_xrootpmap_id);

	trap_errors();

	if (pix && !XGetGeometry(
		tray_data.dpy, pix, &dummy, (int *)&dummy, (int *)&dummy,
		&w, &h, (unsigned int *)&dummy, (unsigned int *)&dummy))
	{
		DBG(0, ("invalid background pixmap\n"));
		pix = None;
	}

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("could not update root background pixmap(%d)\n", trapped_error_code));
		return;
	}
	
	tray_data.bg_pmap = pix;
	tray_data.bg_pmap_width = w;
	tray_data.bg_pmap_height = h;
	
	DBG(4, ("got new root pixmap: 0x%lx (%ix%i)\n", tray_data.bg_pmap, w, h));
}

int get_window_abs_coords(Display *dpy, Window dst, int *x, int *y)
{
	Window root, parent, *wjunk = NULL;
	int x_, y_, x__, y__;
	unsigned int junk;

	trap_errors();

	XGetGeometry(dpy, dst, &root, &x_, &y_, &junk, &junk, &junk, &junk);
	XQueryTree(dpy, dst, &root, &parent, &wjunk, &junk);

	if (junk != 0) XFree(wjunk);

	if (untrap_errors(dpy)) {
		return FAILURE;
	}

	if (parent == root) {
		*x = x_;
		*y = y_;
	} else {
		if (get_window_abs_coords(dpy, parent, &x__, &y__)) {
			*x = x_ + x__;
			*y = y_ + y__;
		} else
			return FAILURE;
	}
	
	return SUCCESS;
}

#ifdef DEBUG
char *get_event_name(int type) 
{
	static char* event_names[35] = {
		"unknown",
		"unknown",
		"KeyPress",
		"KeyRelease",
		"ButtonPress",
		"ButtonRelease",
		"MotionNotify",
		"EnterNotify",
		"LeaveNotify",
		"FocusIn",
		"FocusOut",
		"KeymapNotify",
		"Expose",
		"GraphicsExpose",
		"NoExpose",
		"VisibilityNotify",
		"CreateNotify",
		"DestroyNotify",
		"UnmapNotify",
		"MapNotify",
		"MapRequest",
		"ReparentNotify",
		"ConfigureNotify",
		"ConfigureRequest",
		"GravityNotify",
		"ResizeRequest",
		"CirculateNotify",
		"CirculateRequest",
		"PropertyNotify",
		"SelectionClear",
		"SelectionRequest",
		"SelectionNotify",
		"ColormapNotify",
		"ClientMessage",
		"MappingNotify"
	};

	if (type >= 0 && type < 35)
		return event_names[type];
	else
		return event_names[0];
}

void dump_win_info(Display *dpy, Window wid)
{	
#if 0
	XClassHint xch;
	char cmd[PATH_MAX];
#endif

#if 0
	DBG(4, ("Dumping info for 0x%x\n", wid));

	snprintf(cmd, PATH_MAX, "xwininfo -id 0x%x\n", (unsigned int) wid);
	system(cmd);

	snprintf(cmd, PATH_MAX, "xprop -id 0x%x\n", (unsigned int) wid);
	system(cmd);
#endif
	
#if 0
	xch.res_class = NULL;
	xch.res_name = NULL;

	trap_errors();
	if (XGetClassHint(dpy, wid, &xch)) {
		if (xch.res_class != NULL) DBG(4, ("\tres_class: %s\n", xch.res_class));
		if (xch.res_name != NULL) DBG(4, ("\tres_name: %s\n", xch.res_name));
	}
	untrap_errors(tray_data.dpy);
#endif
}
#endif
