/* 
vim:tabstop=4: 
*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include "debug.h"
#include "icons.h"
#include "xembed.h"

#define PROGNAME PACKAGE


/* from System Tray Protocol Specification 
 * http://freedesktop.org/Standards/systemtray-spec/systemtray-spec-0.1.html */
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define	TRAY_SEL_ATOM "_NET_SYSTEM_TRAY_S"

extern int	gravity;
extern int	vertical;
extern int	icon_size;
extern int	border_width;
extern int	icon_spacing;
extern int	tray_full;
extern int  force;

int			grow_step = 1;
int			grow_flag = 1;
int			parent_bg = 0;
int			show_wnd_deco = 1;

/* just globals */
Display		*dpy;
Window		tray_wnd;

XSizeHints	xsh = {
	flags:	(PSize | PPosition),
	x:		100,
	y:		100,
	width:	50,
	height:	50
};

Atom 		xa_tray_selection;
Atom		xa_tray_opcode;
Atom		xa_tray_data;
Atom 		xa_wm_delete_window;
Atom		xa_wm_protocols;
Atom		xa_wm_take_focus;
Atom 		xa_xembed;
Atom		xa_kde_net_wm_system_tray_window_for;

static int	clean = 0;

Window check_kde_tray_icons(Window w)
{
	/* for XQueryWindow */
	Window root, parent, *children = NULL;
	int nchildren;
	/* for XGetWindowProperty */
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	Window tray_icon_for;

	int s, i;
	Window r;

	trap_errors();

	XGetWindowProperty(dpy, w, xa_kde_net_wm_system_tray_window_for, 0L, sizeof(Window),
			False, XA_WINDOW, &actual_type, &actual_format, &nitems, &bytes_after,
			(unsigned char **) &tray_icon_for);

	if (s = untrap_errors()) {
		debugmsg(DBG_CLS_MAIN, 0, "check_kde_tray_icons(): 0x%x gone?\n", w);
		return None;
	}

	if (actual_type == XA_WINDOW && actual_format != None && tray_icon_for != None) {
		debugmsg(DBG_CLS_MAIN, 4, "check_kde_tray_icons(): 0x%x is KDE tray icon\n", w);
		return w;
	}

	trap_errors();
	XQueryTree(dpy, w, &root, &parent, &children, &nchildren);
	if (s = untrap_errors()) {
		debugmsg(DBG_CLS_MAIN, 4, "check_kde_tray_icons(): 0x%x gone?\n", w);
		if (children != NULL) {
			XFree(children);
		}
		return None;
	}

	for (i = 0; i < nchildren; i++) {
		r = check_kde_tray_icons(children[i]);

		if (r != None) {
			XFree(children);
			return r;
		}
	}

	return None;
}

void cleanup()
{
		/* dummy for now */
		 if (clean)
			return;
		debugmsg(DBG_CLS_MAIN, 0, "cleanup()\n");
		clean_icons(dpy);
		clean = 1;
}

void term_sig_handler(int sig)
{
		cleanup();
		signal(sig, SIG_DFL);
		raise(sig);
}
void grow()
{
	debugmsg(DBG_CLS_MAIN, 3, "grow()\n");

	if (vertical) {
		if (gravity & GRAV_V)
			xsh.y -= grow_step * (icon_spacing + icon_size);
		xsh.height += grow_step * (icon_spacing + icon_size);
	} else {
		if (gravity & GRAV_H) 
			xsh.x -= grow_step * (icon_spacing + icon_size);
		xsh.width += grow_step * (icon_spacing + icon_size);
	}

	debugmsg(DBG_CLS_MAIN, 5, "\tx=%i, y=%i, w=%u, h=%u\n", xsh.x, xsh.y, xsh.width, xsh.height);

	XMoveResizeWindow(dpy, tray_wnd, xsh.x, xsh.y, xsh.width, xsh.height);

	debugmsg(DBG_CLS_MAIN, 3, "grow() out\n");
}

void do_help()
{
	fprintf(stderr, "\nUsage: %s [args]\n"
					"Command line args are:\n"
					"  -b, --background, -bg      set background color\n"
					"  --border-width             set the border size between window edge and icons (default is 2)\n"
					"  -d, --display, -display    launch at specified display\n"
					"  -f, --force-resize         foribly resize icons to fit icon size\n"
					"  -g, -geometry, --geometry  specify geometry\n"
					"  --gravity                  gravity for icons, one of NW, NE, SW, SE\n"
					"  -h, --help                 show this message\n"
					"  -i, --icon-size            set icon size (default is 22)\n"
					"  --icon-spacing             set distance between icons (default is 2)\n"
					"  -n, --nodeco               switch off window decorations\n"
					"  -p, --parentbg             make window parent draw its background\n"
					"  -v, --vertical             layout icons vertically (default is to layout horizontally)\n\n"
					, PROGNAME);
	exit(0);
};

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
	
	char *wnd_name = PROGNAME;
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

	Window		tmp_win;

	char		*tray_sel_atom_name;

	long		mwm_hints[5] = {2, 0, 0, 0, 0};
	Atom		mwm_wm_hints;

	int 		force_relayout;

	char		*dpy_name = NULL;
	char		*bg_color_name = NULL;


	/* register cleanups */
	atexit(cleanup);
	signal(SIGKILL, term_sig_handler);
	signal(SIGTERM, term_sig_handler);
	signal(SIGINT, term_sig_handler);
	
	/* parse cmd line */

	debugmsg(DBG_CLS_MAIN, 20, "argc=%i\n", argc);

	while (--argc > 0) {
		++argv;

		if (!strcmp(argv[0], "-g") || !strcmp(argv[0], "-geometry") ||
			!strcmp(argv[0], "--geometry"))
		{
			argv++;
			argc--;
			if (argc <= 0) {
				fprintf(stderr, "Error: geometry needs an argument\n");
				do_help();
			}
			
			XParseGeometry(argv[0], &xsh.x, &xsh.y, &xsh.width, &xsh.height);
			debugmsg(DBG_CLS_MAIN, 4, "xsh.x=%u, &xsh.y=%u, &xsh.width=%i, xsh.height=%i\n",
					xsh.x, xsh.y, xsh.width, xsh.height)
		}
				

		if (!strcmp(argv[0], "--icon-spacing")) {
			argv++;
			argc--;
			if (argc <= 0) {
				fprintf(stderr, "Error: icon spacing needs an argument\n");
				do_help();
			}
		
			icon_spacing = atoi(argv[0]);
			debugmsg(DBG_CLS_MAIN, 4, "icon_spacing=%i\n", icon_spacing);
			continue;
		}
		
		if (!strcmp(argv[0], "--border-width")) {
			argv++;
			argc--;
			if (argc <= 0) {
				fprintf(stderr, "Error: border width needs an argument\n");
				do_help();
			}

			border_width = atoi(argv[0]);
			debugmsg(DBG_CLS_MAIN, 4, "border_width=%i\n", border_width);
			continue;
		}

		if (!strcmp(argv[0], "-i") || !strcmp(argv[0], "--icon-size")) {
			argv++;
			argc--;
			if (argc <= 0) {
				fprintf(stderr, "Error: icon size needs an argument\n");
				do_help();
			}

			icon_size = atoi(argv[0]);
			if (icon_size < 10) {
					fprintf(stderr, "Warning: icon size reset to default (22)\n");
					icon_size = 22;
			}
			debugmsg(DBG_CLS_MAIN, 4, "icon_size=%i\n", icon_size);
			continue;
		}
		
		if (!strcmp(argv[0], "-f") || !strcmp(argv[0], "--force-resize")) {
			force = 1;
			continue;
		}
		
		if (!strcmp(argv[0], "-h") || !strcmp(argv[0], "--help")) {
			do_help();
		}
		
		if (!strcmp(argv[0], "-p") || !strcmp(argv[0], "--parentbg")) {
			parent_bg = 1;
			continue;
		}

		if (!strcmp(argv[0], "-n") || !strcmp(argv[0], "--nodeco")) {
			show_wnd_deco = 0;
			continue;
		}
		
		if (!strcmp(argv[0], "--gravity")) {
			++argv;
			--argc;

			if (argc <= 0) {
					fprintf(stderr, "Error: gravity needs an argument\n");
					do_help();
			}

			/* FIXME: ok, this is a piece of sh.t, but anyway :) */
			if (strlen(argv[0]) != 2 || 
				!(tolower(argv[0][0]) == 'n' || tolower(argv[0][0]) == 's' ||
				  tolower(argv[0][1]) == 'w' || tolower(argv[0][1]) == 'e'))
			{
				fprintf(stderr, "Error: gravity argument must be one of NE, NW, SE, SW\n");	
				do_help();
			}
			
			gravity = ((tolower(argv[0][1]) == 'e') ? GRAV_E : GRAV_W) | ((tolower(argv[0][0]) == 's') ? GRAV_S : GRAV_N );

			debugmsg(DBG_CLS_MAIN, 4, "gravity=%u\n", gravity);
			continue;
		}

		if (!strcmp(argv[0], "-v") || !strcmp(argv[0], "--vertical")) {
			vertical = 1;
			continue;
		}
		
		if (!strcmp(argv[0], "-b") || !strcmp(argv[0], "--background") ||
			!strcmp(argv[0], "-bg"))
		{
			++argv;
			--argc;
			if (argc <= 0) { 
				fprintf(stderr, "Error: background needs an argument\n");
				do_help();
			}
			bg_color_name = argv[0];
			
			debugmsg(DBG_CLS_MAIN, 4, "bg_color_name=%s(%s) argc=%i\n", bg_color_name, argv[0], argc);
			continue;
		}
		
		if (!strcmp(argv[0], "-d") || !strcmp(argv[0], "--display") || 
			!strcmp(argv[0], "-display"))
		{
			--argc;
			++argv;
			if (argc <= 0) {
				fprintf(stderr, "Error: display needs an argument\n");
				do_help();
			}
			
			dpy_name = argv[0];
			continue;
		}
	}

	
	/* here comes main stuff */
	if ((dpy = XOpenDisplay(dpy_name)) == NULL)
		die("Error: could not open display\n");

	if (bg_color_name != NULL) {
		debugmsg(DBG_CLS_MAIN, 7, "parsing bg\n");
		XParseColor(dpy, XDefaultColormap(dpy, DefaultScreen(dpy)),
					bg_color_name, &xc_bg);
		debugmsg(DBG_CLS_MAIN, 4, "xc_bg.pixel=%u, xc_bg.red=%i, xc_bg.green=%i, xc_bg.blue=%i, xc_bg.flags=%i\n",
						xc_bg.pixel, xc_bg.red, xc_bg.green, xc_bg.blue, xc_bg.flags);
		XAllocColor(dpy, XDefaultColormap(dpy, DefaultScreen(dpy)),
					&xc_bg);
		debugmsg(DBG_CLS_MAIN, 4, "xc_bg.pixel=%u, xc_bg.red=%i, xc_bg.green=%i, xc_bg.blue=%i, xc_bg.flags=%i\n",
						xc_bg.pixel, xc_bg.red, xc_bg.green, xc_bg.blue, xc_bg.flags);
	}
	
	XSynchronize(dpy, True);

	if ((tray_sel_atom_name = (char *)malloc(strlen(TRAY_SEL_ATOM) + 2)) == NULL)
		die("Error: low mem\n");

	snprintf(tray_sel_atom_name, strlen(TRAY_SEL_ATOM) + 2,
		"%s%u", TRAY_SEL_ATOM, DefaultScreen(dpy));

	debugmsg(DBG_CLS_MAIN, 7, "tray_sel_atom_name=%s\n", tray_sel_atom_name);

	xa_tray_selection = XInternAtom(dpy, tray_sel_atom_name, False);

	free(tray_sel_atom_name);
	
	xa_tray_opcode = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	xa_tray_data = XInternAtom(dpy, "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);

	xa_wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	xa_wm_take_focus = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	xa_wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);

	xa_xembed = XInternAtom(dpy, "_XEMBED", False);
	xa_kde_net_wm_system_tray_window_for = XInternAtom(dpy, "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR", False);


	xch.res_class = PROGNAME;
	xch.res_name = PROGNAME;

	tray_wnd = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
					xsh.x, xsh.y, xsh.width, xsh.height, 0, 0, xc_bg.pixel);

	/* XSetWMHints(dpy, tray_wnd, &xwmh); */

	XmbTextListToTextProperty(dpy, &wnd_name, 1, XTextStyle, &wm_name);

	XSetWMProperties(dpy, tray_wnd, &wm_name, NULL, argv, argc, &xsh, &xwmh, &xch);

	mwm_wm_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
	mwm_hints[2] = show_wnd_deco;
	XChangeProperty(dpy, tray_wnd, mwm_wm_hints, mwm_wm_hints, 32, 0, 
			(unsigned char *) mwm_hints, sizeof(mwm_hints));	

	if (parent_bg)
		XSetWindowBackgroundPixmap(dpy, tray_wnd, ParentRelative);

	XSetSelectionOwner(dpy, xa_tray_selection, tray_wnd, CurrentTime);

	if (XGetSelectionOwner(dpy, xa_tray_selection) != tray_wnd)
		die("Error: could not set selection owner.\n\t May be another (greedy) tray running?\n");
	
	xclient_msg32(dpy, RootWindow(dpy, DefaultScreen(dpy)),
					XInternAtom(dpy, "MANAGER", False),
					CurrentTime, xa_tray_selection, tray_wnd, 0, 0);

	XSetWMProtocols(dpy, tray_wnd, &xa_wm_delete_window, 1);
	/* XSetWMProtocols(dpy, tray_wnd, &xa_wm_take_focus, 1); */
	
	XSelectInput(dpy, tray_wnd, SubstructureNotifyMask | StructureNotifyMask | PropertyChangeMask );
	XSelectInput(dpy, DefaultRootWindow(dpy), StructureNotifyMask | SubstructureNotifyMask);
	
	XMapRaised(dpy, tray_wnd);

	XFlush(dpy);

	for(;;) {
		while (XPending(dpy)) {
			XNextEvent(dpy, &ev);
			switch (ev.type) {
			case MapNotify:
				if ((tmp_win = check_kde_tray_icons(ev.xmap.window)) != None) {
					debugmsg(DBG_CLS_MAIN, 0, "Found kde icon 0x%x\n", tmp_win);
					if (tray_full) {
						if (!grow_flag) {
							break;
						} else {
								grow(dpy, tray_wnd);
								force_relayout = 1;
						}
					}

					if (add_icon(dpy, tray_wnd, tmp_win) < 0)
						break;
					
					relayout_icons(dpy, tray_wnd, xsh.width, xsh.height, force_relayout);
					force_relayout = 0;					
				}
				break;	
			case PropertyNotify: /* TODO (non-critical) */
				break;
			case UnmapNotify:	/* window unmapped */
				debugmsg(DBG_CLS_EVENT, 0, "UnmapNotify\n");
				if (ev.xunmap.window != tray_wnd) {
					rem_icon(dpy, tray_wnd, ev.xunmap.window, 0);
					relayout_icons(dpy, tray_wnd, xsh.width, xsh.height, 0);
				}
				break;
				
			case ClientMessage:	/* tray\wm\xembed protocols */
				debugmsg(DBG_CLS_EVENT, 0, "ClientMessage\n");
				if (ev.xclient.message_type == xa_wm_protocols && /* wm */
					ev.xclient.data.l[0] == xa_wm_delete_window)
					{
						exit(0);
					}
				
				if (ev.xclient.message_type == xa_tray_opcode) {
					debugmsg(DBG_CLS_EVENT, 2, "\t_NET_SYSTEM_TRAY_OPCODE, id = %u\n", ev.xclient.data.l[1]);
					switch (ev.xclient.data.l[1]) {
					case SYSTEM_TRAY_REQUEST_DOCK:
						if (tray_full) {
							if (!grow_flag)
								break;
							else {
								grow(dpy, tray_wnd);
								force_relayout = 1;
							}
						}

						if (add_icon(dpy, tray_wnd, ev.xclient.data.l[2]) < 0)
								break;

						relayout_icons(dpy, tray_wnd, xsh.width, xsh.height, force_relayout);
						force_relayout = 0;

						break;
					case SYSTEM_TRAY_BEGIN_MESSAGE:
					case SYSTEM_TRAY_CANCEL_MESSAGE:
					default:
						break;
					}
				}
			
				if (ev.xclient.message_type == xa_xembed)
					debugmsg(DBG_CLS_XEMBED | DBG_CLS_EVENT, 0, "\t_XEMBED message\n");
				break;
				
			case DestroyNotify:	/* window destroyed */
				debugmsg(DBG_CLS_EVENT, 0, "DestroyNotify\n");
				if (ev.xdestroywindow.window == tray_wnd)
					exit(0);
				else
					rem_icon(dpy, tray_wnd, ev.xdestroywindow.window, 0);
				
				break;
				
			case SelectionClear:	/* release system tray selection */
				/* TODO: we must ensure that do not have some job
				 * (that means SYSTEM_TRAY_BEGIN_MESSAGE) in 
				 * progress and quit? or continue living? I dunno...
				 */
				break;
			case ConfigureNotify:	/* window has changed size/pos */
				if (ev.xconfigure.window == tray_wnd) {
					debugmsg(DBG_CLS_EVENT, 0, "ConfigureNotify\n");
					debugmsg(DBG_CLS_EVENT, 5, "\tx = %i, y = %i, width = %u, height = %u\n",
						ev.xconfigure.x, ev.xconfigure.y, ev.xconfigure.width, ev.xconfigure.height);
					if (ev.xconfigure.send_event) {
							xsh.x = ev.xconfigure.x;
							xsh.y = ev.xconfigure.y;
					}
					
					if ((ev.xconfigure.window != tray_wnd) ||
						(xsh.width == ev.xconfigure.width && xsh.height == ev.xconfigure.height))
						break;
					xsh.width = ev.xconfigure.width;
					xsh.height = ev.xconfigure.height;
					relayout_icons(dpy, tray_wnd, xsh.width, xsh.height, 1);
				}
				break;
				
			case ReparentNotify: /* changed parent */
				debugmsg(DBG_CLS_EVENT, 0, "ReparentNotify\n");
				debugmsg(DBG_CLS_EVENT, 3, "\twindow=%u, parent=%u\n", ev.xreparent.window, ev.xreparent.parent);
				if (ev.xreparent.parent == tray_wnd) 
					set_reparented(ev.xreparent.window);

				if (ev.xreparent.window != tray_wnd && ev.xreparent.parent != tray_wnd)
					rem_icon(dpy, tray_wnd, ev.xreparent.window, 1);
	
				break;

			default:
				break;
			}
		}
		usleep(50000L);
	}

	return 0;
}
