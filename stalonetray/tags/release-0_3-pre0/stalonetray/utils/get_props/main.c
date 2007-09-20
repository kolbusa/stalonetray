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
		tmp = XGetAtomName(dpy, atoms[i]);

		fprintf(stdout, "%s\n", tmp);
		
		XFree(tmp);
	}

	fprintf(stdout, "NAMES ----------------------------\n");

	XGetClassHint(dpy, wid, &xch);

	fprintf(stdout, "res_class: %s\nres_name: %s\n", xch.res_class, xch.res_name);
	
	return 0;
}
