/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * ewmh.c
 * Thu, 30 Mar 2006 23:15:37 +0700
 * -------------------------------
 * EWMH support
 * ------------------------------- */
#include <X11/Xmd.h>
#include <X11/Xatom.h>

#include "wmh.h"
#include "common.h"
#include "debug.h"
#include "xutils.h"

#define _NET_WM_STATE_REMOVE        0   /* remove/unset property */
#define _NET_WM_STATE_ADD           1   /* add/set property */
#define _NET_WM_STATE_TOGGLE        2   /* toggle property  */

#define _NET_REQUEST_SOURCE_USER	0
#define _NET_REQUEST_SOURCE_NORMAL	1
#define _NET_REQUEST_SOURCE_PAGER	2

typedef struct {
    CARD32 flags;
    CARD32 functions;
    CARD32 decorations;
    INT32 inputMode;
    CARD32 status;
} PropMotifWmHints;

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

#define PROP_MOTIF_WM_HINTS_ELEMENTS 5

unsigned long	ewmh_list_len = 0;
Atom			xa_net_supported = None;
Atom			*ewmh_list = NULL;

int ewmh_check_support(Display *dpy)
{
	Atom act_type;
	int act_fmt;
	unsigned long bytes_after, prop_len, i;
#ifdef DEBUG
	char *atom_name;
#endif
	
	if (xa_net_supported == None) 
		xa_net_supported = XInternAtom(dpy, "_NET_SUPPORTED", True);

	if (xa_net_supported == None) return FAILURE;
	
	XGetWindowProperty(dpy, DefaultRootWindow(dpy), xa_net_supported,
			0L, 0L, False, XA_ATOM, &act_type, &act_fmt,
			&ewmh_list_len, &bytes_after, (unsigned char **)&ewmh_list);

	if (act_type == None) {
		xa_net_supported = None;
		return FAILURE;
	}

	prop_len = bytes_after / sizeof(Atom);

	XGetWindowProperty(dpy, DefaultRootWindow(dpy), xa_net_supported,
			0L, prop_len, False, XA_ATOM, &act_type, &act_fmt,
			&ewmh_list_len, &bytes_after, (unsigned char **)&ewmh_list);

#ifdef DEBUG
	for (i = 0; i < ewmh_list_len; i++) {
		atom_name = XGetAtomName(dpy, ewmh_list[i]);
		DBG(8, ("_NET_WM_SUPPORTED[%d] = 0x%x (%s)\n", i, ewmh_list[i], atom_name));
		XFree(atom_name);
	}
#endif
	
	return SUCCESS;
}

int ewmh_atom_supported(Atom atom)
{
	int i;
	if (atom == None || xa_net_supported == None) return False;
	for (i = 0; i < ewmh_list_len; i++) 
		if (atom == ewmh_list[i])
			return True;
	return False;
}

int ewmh_add_window_state(Display *dpy, Window wnd, char *state)
{
	Atom prop = XInternAtom(dpy, "_NET_WM_STATE", False);
	Atom atom = XInternAtom(dpy, state, False);
	XWindowAttributes xwa;

#ifdef DEBUG
	char *tmp = XGetAtomName(dpy, atom);
	DBG(8, ("adding window state %s (0x%x)\n", tmp, atom));
	XFree(tmp);
#endif
	
	if (atom == None || prop == None || !ewmh_atom_supported(atom)) return FAILURE;

	trap_errors();
	
	XGetWindowAttributes(dpy, wnd, &xwa);

	if (untrap_errors(dpy)) return FAILURE;

	if (xwa.map_state != IsUnmapped) {
		return send_client_msg32(dpy, DefaultRootWindow(dpy), wnd, prop, 
		                         _NET_WM_STATE_ADD, atom, 0, 0, 0);
	} else {
		trap_errors();

		XChangeProperty(dpy, wnd, prop, XA_ATOM, 32, PropModeAppend, (unsigned char *)&atom, 1);

		if (untrap_errors(dpy))
			return FAILURE;
		else
			return SUCCESS;
	}
}

int ewmh_add_window_type(Display *dpy, Window wnd, char *type)
{
	Atom prop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	Atom atom = XInternAtom(dpy, type, False);
	
	if (atom == None || prop == None || !ewmh_atom_supported(atom)) return FAILURE;
	
	trap_errors();

	XChangeProperty(dpy, wnd, prop, XA_ATOM, 32, PropModeAppend, (unsigned char *)&atom, 1);

	if (untrap_errors(dpy)) {
		DBG(0, ("could append atom to the _NET_WM_WINDOW_TYPE of 0x%x\n", wnd));
		return FAILURE;
	}

	return SUCCESS;
}

#ifdef DEBUG
int ewmh_dump_window_states(Display *dpy, Window wnd)
{
	Atom prop, *data, act_type;
	unsigned long bytes_after, prop_len;
	int act_fmt, j;
	char *tmp;

	prop = XInternAtom(tray_data.dpy, "_NET_WM_STATE", False);

	if (prop == None) return FAILURE;
	
	XGetWindowProperty(dpy, wnd, prop, 0L, 0L, False, XA_ATOM,
			&act_type, &act_fmt, &prop_len, &bytes_after, (unsigned char**)&data);

	prop_len = bytes_after / sizeof(Atom);

	XGetWindowProperty(dpy, wnd, prop, 0L, prop_len,
			False, XA_ATOM, &act_type, &act_fmt, &prop_len, &bytes_after, (unsigned char**)&data);

	for (j = 0; j < prop_len; j++) {
		trap_errors();
		tmp = XGetAtomName(tray_data.dpy, data[j]);
		if (!untrap_errors(tray_data.dpy)) {
			DBG(8, ("0x%x:_NET_WM_STATE[%d] = %s\n", wnd, j, tmp));
			XFree(tmp);
		}
	}

	XFree(data);

	return SUCCESS;
}
#endif

int mwm_set_hints(Display *dpy, Window wnd, unsigned long decorations, unsigned long functions)
{
	PropMotifWmHints *prop, new_prop;
	int act_fmt, rc; 
	unsigned long nitems, bytes_after;
	static Atom atom = None, act_type;
	
	if (atom == None) atom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
	if (atom == None) return FAILURE;

	trap_errors();

	rc = XGetWindowProperty(dpy, wnd, atom, 0, 5, False, atom, 
				&act_type, &act_fmt, &nitems, &bytes_after, 
				(unsigned char**) &prop);

	/* the hint is either not set or has some other type */
	if ((act_type == None && act_fmt == 0 && bytes_after == 0) || nitems == 0) {
		memset(&new_prop, 0, sizeof(PropMotifWmHints));
		prop = &new_prop;
	} else if (prop != NULL) {
		new_prop = *prop;
		XFree(prop);
		prop = &new_prop;
	} else {
		untrap_errors(dpy);
		return FAILURE;
	}
	
	prop->flags |= MWM_HINTS_DECORATIONS | MWM_HINTS_FUNCTIONS;
	prop->decorations = decorations;
	prop->functions = functions;

	XChangeProperty(dpy, wnd, atom, atom, 32, PropModeReplace, 
			(unsigned char*) prop, PROP_MOTIF_WM_HINTS_ELEMENTS);

	if (untrap_errors(dpy)) 
		return FAILURE;
	else
		return SUCCESS;
}

