/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * main.c 
 * Tue, 24 Aug 2004 12:00:24 +0700
 * ------------------------------- 
 * main is main
 * ------------------------------- */

/* TODO: XEMBED Implementation wanted */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <stdlib.h>
#include <stdio.h>

int trapped_error_code = 0;
static int (*old_error_handler) (Display *, XErrorEvent *);

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



void die(char *msg)
{
	fprintf(stderr, "%s", msg);
	exit(1);
}

int main(int argc, char** argv)
{	
	Display	*dpy;
	Window wid;
	XClassHint xch;

	char *tmp;
	
	Atom *atoms;
	int nprops, i;

	wid = strtol(argv[1], &tmp, 0);

	if ((dpy = XOpenDisplay(NULL)) == NULL) {
		die("Error: could not open display\n");
	}

	XSynchronize(dpy, True);
	
	atoms = XListProperties(dpy, wid, &nprops);
	
	fprintf(stdout, "ATOMS ----------------------------\n");

	for (i = 0; i < nprops; i++) {
		Atom act_type, *atom_list;
		int act_fmt, j;
		short *sints;
		long *lints;
		unsigned long prop_len, bytes_after;
		unsigned char *prop, *aname;
		
		tmp = XGetAtomName(dpy, atoms[i]);
		fprintf(stdout, "Atom #%d\n  name: '%s'\n", i, tmp);

		XGetWindowProperty(dpy, wid, atoms[i], 0L, 0L, False, AnyPropertyType,
				&act_type, &act_fmt, &prop_len, &bytes_after, &prop);

		prop_len = bytes_after / (act_fmt == 8 ? 1 : (act_fmt == 16 ? 2 : 4));

		XGetWindowProperty(dpy, wid, atoms[i], 0L, prop_len, False, act_type,
				&act_type, &act_fmt, &prop_len, &bytes_after, &prop);

		fprintf(stdout, "  size: %u\n", prop_len);
		
		switch (act_fmt) {
			case 8:
				fprintf(stdout, "  possible content type is string: %s\n", prop);
				break;
			case 16:
				fprintf(stdout, "  possible content type is list of short ints:\n");
				sints = prop;
				for (j = 0; j < prop_len; j++)
					fprintf(stdout, "    %s[%i] = %d\n", tmp, j, sints[j]);
				break;
			case 32:
				fprintf(stdout, "  possible content type is list of long ints:\n");
				lints = prop;
				for (j = 0; j < prop_len; j++)
					fprintf(stdout, "    %s[%i] = %ld\n", tmp, j, lints[j]);
				
				fprintf(stdout, "  possible content type is list atoms:\n");
				atom_list = prop;
				for (j = 0; j < prop_len; j++) {
					trap_errors();
					aname = (unsigned char*)XGetAtomName(dpy, atom_list[j]);
					if (!untrap_errors(dpy) && aname != NULL) {
						fprintf(stdout, "    %s[%i] = %s\n", tmp, j, aname);
						XFree(aname);
					}
				}

				break;
			default:
				fprintf(stdout, "  WOW: unknown format: %u\n", act_fmt);
				break;
		}

		XFree(prop);
		XFree(tmp);
	}

	fprintf(stdout, "NAMES ----------------------------\n");

	XGetClassHint(dpy, wid, &xch);

	fprintf(stdout, "res_class: %s\nres_name: %s\n", xch.res_class, xch.res_name);


	return 0;
}
