/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * tray.h
 * Mon, 09 Mar 2009 12:41:32 -0400
 * -------------------------------
 * Scrollbars data & interface
 * -------------------------------*/

#ifndef _SCROLLBAR_H_
#define _SCROLLBAR_H_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define SB_WND_TOP	0
#define SB_WND_BOT	1
#define SB_WND_LFT	2
#define SB_WND_RHT	3
#define SB_WND_MAX	4

#define SB_MODE_NONE 0
#define SB_MODE_VERT 1
#define SB_MODE_HORZ 2

#define SCROLLBAR_REPEAT_COUNTDOWN_MAX_1ST 10
#define SCROLLBAR_REPEAT_COUNTDOWN_MAX_SUCC 5

/* Scrollbar data */
struct ScrollbarsData {
	struct Point scroll_base;				/* Base scroll position */
	struct Point scroll_pos;                /* Current scroll position */
	Window scrollbar[SB_WND_MAX];           /* Window IDs of scrollbars */
	XSizeHints scrollbar_xsh[SB_WND_MAX];	/* Cached window sizes for scrollbars */
	int scrollbar_down;						/* Click state */
	int scrollbar_highlighted;				/* Highlight state */
	int scrollbar_repeat_active;			/* If repeat is active */
	int scrollbar_repeat_counter;			/* Countown for repeat action */
	int scrollbar_repeats_done;				/* Numberf of executed repeat actions */
};

/* Initialize data structures */
void scrollbars_init();
/* Create scrollbars windows */
void scrollbars_create();
/* Update positions of scrollbars */
int scrollbars_update();
/* Refresh scrollbars */
int scrollbars_refresh(int exposures);
/* Get scrollbar under given coords */
int scrollbars_get_id(Window wid, int x, int y);
/* Update tray wrt scrollbar click;
 *  - id == SB_WND_MAX is a special case used to
 *    update scroll positions without chaning it 
 *    (i.e. after icon removal)
 */
int scrollbars_click(int id);
/* Event handler for scrollbars */
void scrollbars_handle_event(XEvent ev);
/* Perform periodic tasks for scrollbars */
void scrollbars_periodic_tasks();
/* Scroll to icon */
int scrollbars_scroll_to(struct TrayIcon *ti);
/* Highlight scrollbar */
int scrollbars_highlight_on(int id);
/* Switch hightlight off */
int scrollbars_highlight_off(int id);
#endif 
