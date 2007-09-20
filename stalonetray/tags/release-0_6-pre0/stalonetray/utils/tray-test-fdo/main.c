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

/* from System Tray Protocol Specification 
 * http://freedesktop.org/Standards/systemtray-spec/systemtray-spec-0.1.html */
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define	TRAY_SEL_ATOM "_NET_SYSTEM_TRAY_S"

#include "xembed.h"

#define GROW_PERIOD 15

/* just globals */
Display		*dpy;
Window		wnd;
Window 		tray;
int 		reparent_out = 0;
int			grow = 0;
int			grow_cd = GROW_PERIOD;
int			maintain_size = 1;

XSizeHints	xsh = {
	flags:	(PSize | PPosition),
	x:		100,
	y:		100,
	width:	20,
	height:	20
};

Atom 		xa_tray_selection;
Atom		xa_tray_opcode;
Atom		xa_tray_data;
Atom 		xa_wm_delete_window;
Atom		xa_wm_protocols;
Atom		xa_wm_take_focus;
Atom 		xa_xembed;

void die(char *msg)
{
	fprintf(stderr, "%s", msg);
	exit(1);
}

int main(int argc, char** argv)
{
	XClassHint	xch;
	XWMHints	xwmh = {
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
	
	char *wnd_name = "test_tray_icon";
	XTextProperty wm_name;

	XWindowAttributes xwa;
	XEvent		ev;
	XColor		xc_bg = {
		flags:	0,
		pad:	0,
		pixel:	0x777777,
		red:	0,
		green:	0,
		blue:	0
	};

	char		*tray_sel_atom_name;
	char		*tmp;

	long		mwm_hints[5] = {2, 0, 0, 0, 0};
	Atom		mwm_wm_hints;

	char		*dpy_name = NULL;
	char		*bg_color_name = NULL;

	while (--argc > 0) {
		++argv;
		
		if (!strcmp(argv[0], "-w")) {
			argv++;
			argc--;
			xsh.width = strtol(argv[0], &tmp, 0);
		}
	
		if (!strcmp(argv[0], "-h")) {
			argv++;
			argc--;
			xsh.height = strtol(argv[0], &tmp, 0);
		}

		if (!strcmp(argv[0], "-r")) {
			argv++;
			argc--;
			reparent_out = 1;
		}

		if (!strcmp(argv[0], "-c")) {
			argv++;
			argc--;
			bg_color_name = argv[0];
		}

		if (!strcmp(argv[0], "-g")) {
			grow = 1;
		}

		if (!strcmp(argv[0], "-gg")) {
			grow = 2;
		}

		if (!strcmp(argv[0], "-n")) {
			maintain_size = 0;
		}
	}


	if ((dpy = XOpenDisplay(dpy_name)) == NULL) {
		die("Error: could not open display\n");
	}

	if (bg_color_name != NULL) {
		XParseColor(dpy, XDefaultColormap(dpy, DefaultScreen(dpy)),
					bg_color_name, &xc_bg);
		XAllocColor(dpy, XDefaultColormap(dpy, DefaultScreen(dpy)),
					&xc_bg);
	}


	XSynchronize(dpy, True);

	if ((tray_sel_atom_name = (char *)malloc(strlen(TRAY_SEL_ATOM) + 2)) == NULL) {
		die("Error: low mem\n");
	}

	snprintf(tray_sel_atom_name, strlen(TRAY_SEL_ATOM) + 2,
		"%s%u", TRAY_SEL_ATOM, DefaultScreen(dpy));

	xa_tray_selection = XInternAtom(dpy, tray_sel_atom_name, False);

	free(tray_sel_atom_name);
	
	xa_tray_opcode = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	xa_tray_data = XInternAtom(dpy, "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);

	xa_xembed = XInternAtom(dpy, "_XEMBED", False);

	xch.res_class = wnd_name;
	xch.res_name = wnd_name;

	trap_errors();	
	
	wnd = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
					xsh.x, xsh.y, xsh.width, xsh.height, 0, 0, xc_bg.pixel);

	if (untrap_errors()) {
		die("Error: could not create simple window\n");
	} else {
		printf("wid=0x%x\n", wnd);
	}

	XmbTextListToTextProperty(dpy, &wnd_name, 1, XTextStyle, &wm_name);

	XSetWMProperties(dpy, wnd, &wm_name, NULL, argv, argc, &xsh, &xwmh, &xch);

	if ((tray = XGetSelectionOwner(dpy, xa_tray_selection)) == None) {
		printf("Error: no tray found\n");
	} else {
		xclient_msg32(dpy, tray, xa_tray_opcode, CurrentTime, SYSTEM_TRAY_REQUEST_DOCK, wnd, 0, 0);
	}

	XSelectInput(dpy, wnd, SubstructureNotifyMask | StructureNotifyMask | PropertyChangeMask );
	
	XMapRaised(dpy, wnd);	

	XFlush(dpy);

	printf("::::: wid: 0x%x, color: %s\n", wnd, bg_color_name != NULL ? bg_color_name : "gray");

	if (reparent_out) {
		usleep(5000000L);
		fflush(stdout);
		printf("reparenting out\n");
		fflush(stdout);
		XReparentWindow(dpy, wnd, DefaultRootWindow(dpy), 100, 100);
	}
	
	for(;;) { while(XPending(dpy)) {
		XNextEvent(dpy, &ev);
		switch(ev.type) {
		case DestroyNotify:
			return 0;
		case ConfigureNotify:
			if (maintain_size && (ev.xconfigure.width != xsh.width || ev.xconfigure.height != xsh.height)) {
				printf("0x%x: configured: %dx%d\n", wnd, ev.xconfigure.width, ev.xconfigure.height);
				printf("0x%x: maintaining size\n", wnd);
				XResizeWindow(dpy, wnd, xsh.width, xsh.height);
			}
		default:
			break;
		}
	}
	
	if (grow == 1) {
		grow_cd--;
		printf("grow countdown: %d\n", grow_cd);
		if (!grow_cd) {
			XResizeWindow(dpy, wnd, xsh.width * 2, xsh.height * 2);
			xsh.width *= 2;
			xsh.height *= 2;
			grow = 0;
		}
	} else if (grow == 2) {
		grow_cd--;
		printf("grow countdown: %d\n", grow_cd);
		if (grow_cd == 0) {
			XResizeWindow(dpy, wnd, xsh.width * 2, xsh.height * 2);
			xsh.width *= 2;
			xsh.height *= 2;
		} else if (grow_cd == -GROW_PERIOD) {
			xsh.width /= 2;
			xsh.height /= 2;
			XResizeWindow(dpy, wnd, xsh.width, xsh.height);
		}
	}

	usleep(50000L);	}
	
	
	return 0;
}
