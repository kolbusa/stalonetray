/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * icons.h
 * Tue, 24 Aug 2004 12:05:38 +0700
 * -------------------------------
 * Manipulations with the list of
 * tray icons
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
	struct Rect grd_rect;	/* the rect in the grid */
	struct Rect icn_rect;	/* real position inside the tray */
	struct Point wnd_sz;	/* size of the window of the icon */
};

struct TrayIcon {
	struct TrayIcon *next;
	struct TrayIcon *prev;
	Window wid; 				/* the window itself */
	Window mid_parent; 			/* mid-parent */
	int cmode; 					/* compatibility mode: CM_FDO/CM_KDE (see embed.h) */
	int is_embedded;			/* flag: is the icon succesfully embedded ? */
	int is_invalid;				/* flag: is the icon invalid ? */
	int is_hidden;     			/* flag: is the icon hidden ? */
	int is_resized;				/* flag: the icon has recently resized itself */
	int is_layed_out;			/* flag: the icon is succesfully layed out */
	int is_updated;				/* flag: the position of the icon needs to be updated */
	int is_xembed_supported;	/* flag: does the icon support xembed */
	int is_size_set;			/* flag: has the size for the icon been set */
	int is_xembed_wants_focus;	/* flag: does the icon want focus */
	long xembed_last_timestamp; /* the timestamp of last processed xembed message */
	long xembed_last_msgid; 	/* id of the last processed xembed message */
	struct Layout l;	 		/* layout info */
};

typedef int (*IconCmpFunc) (struct TrayIcon *, struct TrayIcon *); 
typedef int (*IconCallbackFunc) (struct TrayIcon *);

/* adds the new icon to the list */
struct TrayIcon *allocate_icon(Window w, int cmode);

/* deletes the icon from the list */
int deallocate_icon(struct TrayIcon *ti);

/* returns next/previous icon in the list */
struct TrayIcon *next_icon(struct TrayIcon *ti);
struct TrayIcon *prev_icon(struct TrayIcon *ti);

/************************************************
 * BIG FAT WARNING: backup/restore routines will 
 * memleak/fail if the number of icons  in the 
 * list has changed between backup/restore calls.
 * (in return, it does not invalidate pointers :P)
 *************************************************/

/* backs up the list */
int icons_backup();

/* restores the list from the backup */
int icons_restore();

/* frees the back-up list */
int icons_purge_backup();

/*************************************************/

/* applies the callback to all icons */
struct TrayIcon *forall_icons(IconCallbackFunc cbk);

/* applies the callback to all icons starting from the tgt */
struct TrayIcon *forall_icons_from(struct TrayIcon *tgt, IconCallbackFunc cbk);

/* clears the whole list */
int clean_icons();
/* clears the whole list, calling cbk for each icon */
int clean_icons_cbk(IconCallbackFunc cbk);

/* sorts icons */
/* if a < b => cmp(a,b) < 0
 * if a = b => cmp(a,b) = 0
 * if a > b => cmp(a,b) > 0 */
void sort_icons(IconCmpFunc cmp);

/* finds the list item corresponding to w */
struct TrayIcon *find_icon(Window w);

/* finds the list item corresponding to w
 * (icon window or mid-parent) */
struct TrayIcon *find_icon_ex(Window w);

/* prints info on the icon */
int print_icon_data(struct TrayIcon *ti);

#endif
