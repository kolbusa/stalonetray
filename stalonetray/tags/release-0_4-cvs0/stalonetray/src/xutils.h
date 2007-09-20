/* ************************************
 * vim:tabstop=4:shiftwidth=4
 * xutils.h
 * Sun, 05 Mar 2006 17:16:44 +0600
 * ************************************
 * misc X11 utilities
 * ************************************/

#ifndef _XUTILS_H_
#define _XUTILS_H_

#include <X11/X.h>

#include "icons.h"

struct EventData {
	int type;
	Window w;
};

int wait_for_event_serial(Window w, int type, unsigned long *serial);
int wait_for_event(Window w, int type);

extern int trapped_error_code;

/*#define TrayXAtom(atom)	XInternAtom(tray_data.dpy, atom, False)*/

void trap_errors();
int untrap_errors(Display *dpy);

Time get_server_timestamp(Display *dpy, Window wnd);

int send_client_msg32(Display *dpy, Window dst, Window wnd,
                      Atom type, long data0, long data1,
                      long data2, long data3, long data4);

int icon_repaint(struct TrayIcon *ti);

int hide_window(struct TrayIcon *ti);
int show_window(struct TrayIcon *ti);

int set_window_size(struct TrayIcon *ti, struct Point dim);
int get_window_size(struct TrayIcon *ti, struct Point *res);

int get_window_abs_coords(Display *dpy, Window dst, int *x, int *y);
	
void update_root_pmap();

#ifdef DEBUG
void dump_win_info(Display *dpy, Window w);
#endif

#endif

