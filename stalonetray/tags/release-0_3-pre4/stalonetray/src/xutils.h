/* ************************************
 * vim:tabstop=4:shiftwidth=4
 * xutils.h
 * Sun, 05 Mar 2006 17:16:44 +0600
 * ************************************
 * X11 utilities
 * ************************************/

#ifndef _XUTILS_H_
#define _XUTILS_H_

extern int trapped_error_code;

void trap_errors();

int untrap_errors(Display *dpy);

int send_client_msg32(Display *dpy, Window dst, Atom type, 
                      long data0, long data1, long data2,
					  long data3, long data4);

int icon_repaint(struct TrayIcon *ti);


int hide_window(struct TrayIcon *ti);
int show_window(struct TrayIcon *ti);

int set_window_size(struct TrayIcon *ti, struct Point dim);
int get_window_size(struct TrayIcon *ti, struct Point *res);
	
void update_root_pmap();

#endif

