/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* xembed.c
* Tue, 24 Aug 2004 12:05:38 +0700
* -------------------------------
* XEMBED protocol implementation
* -------------------------------*/

#include <X11/Xmd.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "common.h"
#include "xembed.h"
#include "xutils.h"

extern Atom xa_xembed;
extern Atom xa_xembed_info;

int xembed_retrive_data(Display *dpy, Window w, CARD32 *data)
{
	Atom act_type;
	int act_fmt;
	unsigned long nitems, bytesafter;
	unsigned char *tmpdata;
	int rc;
	XGetWindowProperty(dpy,
					   w,
					   xa_xembed_info,
					   0,
					   2,
					   False,
					   xa_xembed_info,
					   &act_type,
					   &act_fmt,
					   &nitems,
					   &bytesafter,
					   &tmpdata);

	if (!x11_ok()) return XEMBED_RESULT_X11ERROR;
	rc = (x11_ok() && act_type == xa_xembed_info && nitems == 2);
	if (rc) {
		data[0] = ((CARD32 *) tmpdata)[0];
		data[1] = ((CARD32 *) tmpdata)[1];
	}
	if (nitems && tmpdata != NULL) XFree(tmpdata);
	return rc ? XEMBED_RESULT_OK : XEMBED_RESULT_UNSUPPORTED;
}

int xembed_post_data(Display *dpy, Window w, CARD32 *data)
{
	XChangeProperty(dpy,
					w,
					xa_xembed_info,
					xa_xembed_info,
					32,
					PropModeReplace,
					(unsigned char *) data,
	                2);
	return x11_ok() ? XEMBED_RESULT_OK : XEMBED_RESULT_X11ERROR;
}

