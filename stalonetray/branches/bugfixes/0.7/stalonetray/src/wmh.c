/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * ewmh.c
 * Thu, 30 Mar 2006 23:15:37 +0700
 * -------------------------------
 * EWMH/MWM support
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

/* Structure for Motif WM hints */
typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
} PropMotifWmHints;
/* Bit flags for fields of MWM hints data structure */
#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)
/* Number of CARD32 entries in MWM hints data structure */
#define PROP_MOTIF_WM_HINTS_ELEMENTS 5
/* List of all EWMH atoms supoorted by WM */
Atom			*ewmh_list = NULL;
/* Length of previously-mentioned list */
unsigned long	ewmh_list_len = 0;
/* Atom: _NET_SUPPORTED */
Atom			xa_net_supported = None;

/* Check if WM supports EWMH */
int ewmh_check_support(Display *dpy)
{
	unsigned long i;
#ifdef DEBUG
	char *atom_name;
#endif

	/* Check for presence of _NET_SUPPORTED atom */
	if (xa_net_supported == None) 
		xa_net_supported = XInternAtom(dpy, "_NET_SUPPORTED", True);
	if (xa_net_supported == None) return FAILURE;

	/* Retrive the value of _NET_SUPPORTED property of root window */
	if (!x11_get_win_prop32(dpy, DefaultRootWindow(dpy), xa_net_supported, XA_ATOM, (unsigned char **) &ewmh_list, &ewmh_list_len)) {
		xa_net_supported = None;
		return FAILURE;
	}

#ifdef DEBUG
	for (i = 0; i < ewmh_list_len; i++) {
		atom_name = XGetAtomName(dpy, ewmh_list[i]);
		DBG(8, ("_NET_WM_SUPPORTED[%d] = 0x%x (%s)\n", i, ewmh_list[i], atom_name));
		XFree(atom_name);
		x11_ok();
	}
#endif
	
	return SUCCESS;
}

/* Check if atom is in the list of supported EWMH atoms */
int ewmh_atom_supported(Atom atom)
{
	int i;
	if (atom == None || xa_net_supported == None) return False;
	for (i = 0; i < ewmh_list_len; i++) 
		if (atom == ewmh_list[i])
			return True;
	return False;
}

/* Add EWMH window state for the given window */
int ewmh_add_window_state(Display *dpy, Window wnd, char *state)
{
	Atom prop; 
	Atom atom;
	XWindowAttributes xwa;
	int rc;

	/* Check if WM supports EWMH window states */
	prop = XInternAtom(dpy, "_NET_WM_STATE", True);
	atom = XInternAtom(dpy, state, True);
	if (atom == None || prop == None || !ewmh_atom_supported(atom)) return FAILURE;

	DBG(8, ("adding window state %s (0x%x)\n", state, atom));

	rc = XGetWindowAttributes(dpy, wnd, &xwa);
	if (!x11_ok() || !rc ) return FAILURE;

	if (xwa.map_state != IsUnmapped) {
		/* For unmapped windows, ask WM to add the window state */
		return x11_send_client_msg32(dpy, DefaultRootWindow(dpy), wnd, prop, 
		                             _NET_WM_STATE_ADD, atom, 0, 0, 0);
	} else {
		/* Else, add the window state ourselves */
		XChangeProperty(dpy, wnd, prop, XA_ATOM, 32, PropModeAppend, (unsigned char *)&atom, 1);
		return x11_ok();
	}
}

/* Add EWMH window type for the given window */
int ewmh_add_window_type(Display *dpy, Window wnd, char *type)
{
	Atom prop;
	Atom atom;
	/* Check if WM supports supports _NET_WM_WINDOW_TYPE and requested window type */
	prop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", True);
	atom = XInternAtom(dpy, type, True);
	if (atom == None || prop == None || !ewmh_atom_supported(atom)) return FAILURE;
	
	/* Update property value (append) */
	XChangeProperty(dpy, wnd, prop, XA_ATOM, 32, PropModeAppend, (unsigned char *)&atom, 1);

	if (!x11_ok()) {
		DBG(0, ("could append atom to the _NET_WM_WINDOW_TYPE list property of 0x%x\n", wnd));
		return FAILURE;
	}

	return SUCCESS;
}

#ifdef DEBUG
/* List EWMH states that are set for the given window */
int ewmh_dump_window_states(Display *dpy, Window wnd)
{
	Atom prop, *data;
	unsigned long prop_len;
	int j;
	char *tmp;

	/* Check if WM supports _NET_WM_STATE */
	prop = XInternAtom(tray_data.dpy, "_NET_WM_STATE", True);
	if (prop == None) return FAILURE;

	/* Retrive the list of states */
	if (x11_get_win_prop32(dpy, wnd, prop, XA_ATOM, (unsigned char**) &data, &prop_len)) {
		for (j = 0; j < prop_len; j++) {
			tmp = XGetAtomName(tray_data.dpy, data[j]);
			if (x11_ok() && tmp != NULL) {
				DBG(8, ("0x%x:_NET_WM_STATE[%d] = %s\n", wnd, j, tmp));
				XFree(tmp);
			}
		}
		return SUCCESS;
	}

	return FAILURE;
}
#endif

/* Set MWM hints */
int mwm_set_hints(Display *dpy, Window wnd, unsigned long decorations, unsigned long functions)
{
	PropMotifWmHints *prop = NULL, new_prop;
	int act_fmt, rc; 
	unsigned long nitems, bytes_after;
	static Atom atom = None, act_type;

	/* Check if WM supports Motif WM hints */
	if (atom == None) atom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
	if (atom == None) return FAILURE;

	/* Get current hints */
	rc = XGetWindowProperty(dpy, wnd, atom, 0, 5, False, atom, 
				&act_type, &act_fmt, &nitems, &bytes_after, 
				(unsigned char**) &prop);

	if ((act_type == None && act_fmt == 0 && bytes_after == 0) || nitems == 0) {
		/* Hints are either not set or have some other type.
		 * Reset all values. */
		memset(&new_prop, 0, sizeof(PropMotifWmHints));
		if (prop != NULL) XFree(prop);
		prop = &new_prop;
	} else if (prop != NULL) {
		/* Copy value */
		new_prop = *prop;
		XFree(prop);
		prop = &new_prop;
	} else {
		/* Something is broken */
		x11_ok(); /* Reset x11 error status */
		return FAILURE;
	}

	/* Update value */
	prop->flags |= MWM_HINTS_DECORATIONS | MWM_HINTS_FUNCTIONS;
	prop->decorations = decorations;
	prop->functions = functions;

	XChangeProperty(dpy, wnd, atom, atom, 32, PropModeReplace, 
			(unsigned char*) prop, PROP_MOTIF_WM_HINTS_ELEMENTS);

	return x11_ok();
}

