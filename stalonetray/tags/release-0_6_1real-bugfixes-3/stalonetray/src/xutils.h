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

#include "common.h"
#include "icons.h"

int wait_for_event_serial(Window w, int type, unsigned long *serial);
int wait_for_event(Window w, int type);

Time get_server_timestamp(Display *dpy, Window wnd);

int send_client_msg32(Display *dpy, Window dst, Window wnd,
                      Atom type, long data0, long data1,
                      long data2, long data3, long data4);

int repaint_icon(struct TrayIcon *ti);

int set_window_size(struct TrayIcon *ti, struct Point dim);
int get_window_size(struct TrayIcon *ti, struct Point *res);
int get_window_min_size(struct TrayIcon *ti, struct Point *res);

int get_root_window_prop(Display *dpy, Atom atom, Window **data, unsigned long *len);

int get_window_abs_coords(Display *dpy, Window dst, int *x, int *y);
	
void update_root_pmap();

extern int trapped_error_code;

#ifdef DEBUG
void trap_errors_real(char *, int, char*);
#define trap_errors() trap_errors_real(__FILE__, __LINE__, (char *)PORT_FUNC)
#else
void trap_errors_real();
#define trap_errors() trap_errors_real()
#endif

int untrap_errors(Display *dpy);

#ifdef DEBUG
char *get_event_name(int type);
void dump_win_info(Display *dpy, Window w);
#endif

#endif

