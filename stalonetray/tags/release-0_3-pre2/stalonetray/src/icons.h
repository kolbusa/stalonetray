/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * icons.h
 * Tue, 24 Aug 2004 12:05:38 +0700
 * -------------------------------
 * Manipulations with reparented 
 * windows --- tray icons 
 * -------------------------------*/

#ifndef _ICONS_H_
#define _ICONS_H_

#include "config.h"

struct Point {
	int x, y;
};
struct Rect {
	int x, y, w, h;
};

#define RX1(r) (r.x)
#define RY1(r) (r.y)
#define RX2(r) (r.x + r.w - 1)
#define RY2(r) (r.y + r.h - 1)

struct Layout {
	struct Rect grd_rect, icn_rect;
	struct Point wnd_sz;

	Window p;

	int resized;
	int layed_out;
	int update_pos;
};

struct TrayIcon {
	struct TrayIcon *next;
	struct TrayIcon *prev;

	Window w;  /* the window itself */
	
	int cmode; /* compatibility mode --- KDE\XEMBED\etc */
	int embeded; /* just a flag */
	int invalid;
	
	struct Layout l;  /* icon's layout info */
};

typedef int (*IconCmpFunc) (struct TrayIcon *, struct TrayIcon *); 
typedef int (*IconCallbackFunc) (struct TrayIcon *);

/* adds the new icon to the list */
struct TrayIcon *add_icon(Window w, int cmode);

/* deletes the icon from the list */
int del_icon(struct TrayIcon *ti);

/* backs up the list */
int backup_icons();

/* restores the list from the backup */
int restore_icons();

/* applies the callback to all icons */
struct TrayIcon *forall_icons(IconCallbackFunc cbk);

/* applies the callback to all icons starting from the tgt */
struct TrayIcon *forall_icons_from(struct TrayIcon *tgt, IconCallbackFunc cbk);

/* clears the whole list */
int clean_icons(IconCallbackFunc cbk);

/* sorts icons */
/* if a < b => cmp(a,b) < 0
 * if a = b => cmp(a,b) = 0
 * if a > b => cmp(a,b) > 0 */
void sort_icons(IconCmpFunc cmp);

/* finds the list item corresponding to w */
struct TrayIcon *find_icon(Window w);

#ifdef DEBUG
int print_icon_data(struct TrayIcon *ti);
#endif

#endif
