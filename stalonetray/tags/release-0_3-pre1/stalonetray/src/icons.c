/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* icons.c
* Tue, 24 Aug 2004 12:05:38 +0700
* -------------------------------
* Manipulations with reparented 
* windows --- tray icons 
* -------------------------------*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "icons.h"

#include "debug.h"
#include "layout.h"
#include "xembed.h"
#include "list.h"
#include "tray.h"

struct TrayIcon *icons_head = NULL;

struct TrayIcon *add_icon(Window w, int cmode)
{
	struct TrayIcon *new_icon, *tmp;

	if (find_icon(w) != NULL)
		return NULL;
	
	if ((new_icon = (struct TrayIcon *) malloc(sizeof(struct TrayIcon))) == NULL) {
		ERR("Out of memory");
		return NULL;
	}

	new_icon->w = w;
	new_icon->l.layed_out = 0;
	new_icon->l.wnd_sz.x = 0;
	new_icon->l.wnd_sz.y = 0;
	new_icon->embeded = 0;
	new_icon->l.p = 0;
	new_icon->l.update_pos = 0;
	new_icon->l.resized = 0;
	new_icon->cmode = cmode;
	new_icon->invalid = 1;

	LIST_ADD_ITEM(icons_head, new_icon);
	return new_icon;
}

int del_icon(struct TrayIcon *ti)
{
	LIST_DEL_ITEM(icons_head, ti);
	free(ti);
	return SUCCESS;
}

struct TrayIcon *find_icon(Window w)
{
	struct TrayIcon *tmp;
	for (tmp = icons_head; tmp != NULL; tmp = tmp->next)
		if (tmp->w == w)
			return tmp;

	return NULL;
}

int clean_icons(IconCallbackFunc cbk)
{
	struct TrayIcon *tmp;
	LIST_CLEAN_CBK(icons_head, tmp, cbk);
}	

/* XXX: can _surely_ be optimized */
void sort_icons(IconCmpFunc cmp)
{
	struct TrayIcon *new_head = NULL, *cur, *tmp;
	while (icons_head != NULL) {
		cur = icons_head;
		for (tmp = icons_head; tmp != NULL; tmp = tmp->next)
			if (cmp(tmp, cur) > 0)
				cur = tmp;

		LIST_DEL_ITEM(icons_head, cur);
		LIST_ADD_ITEM(new_head, cur);
	}

	icons_head = new_head;
#ifdef DEBUG
	forall_icons(&print_icon_data);
#endif
}

struct TrayIcon *forall_icons(IconCallbackFunc cbk)
{
	forall_icons_from(icons_head, cbk);
}


struct TrayIcon *forall_icons_from(struct TrayIcon *tgt, IconCallbackFunc cbk)
{
	struct TrayIcon *tmp = icons_head;
	for (tmp = tgt; tmp != NULL; tmp = tmp->next) 
		if (cbk(tmp)) {
			return tmp;
		}

	return NULL;
}

void place_icon(struct TrayIcon *ti, IconCmpFunc cmp)
{
	struct TrayIcon *tmp;

	LIST_DEL_ITEM(icons_head, ti);

	for (tmp = icons_head; tmp->next != NULL; tmp = tmp->next) {
		if (cmp(ti, tmp) > 0) {
			LIST_INSERT_AFTER(icons_head, tmp->prev, ti);
			return;
		}
	}

	LIST_ADD_ITEM(icons_head, ti);
}

#ifdef DEBUG
int print_icon_data(struct TrayIcon *ti)
{
	XWindowAttributes xwa;

	DBG(4, "ptr = %p\n", ti);
	DBG(4, "  prev = %p\n", ti->prev);
	DBG(4, "  next = %p\n", ti->next);
	DBG(4, "  wid = 0x%x\n", ti->w);
	DBG(4, "  invalid = %d\n", ti->invalid);
	if (ti->embeded) {
		trap_errors();
	
		XGetWindowAttributes(tray_data.dpy, ti->l.p, &xwa);
	
		if (untrap_errors(tray_data.dpy)) {
			DBG(4, "  WEIRD: could not get mid-parents stats (%d)\n", trapped_error_code);
		} else {
			DBG(4, "  mid-parent wid = 0x%x\n", ti->l.p);
			DBG(4, "  parent geometry = %dx%d+%d+%d\n", xwa.width, xwa.height, xwa.x, xwa.y);
		}
	} else {
		DBG(4, "  not embedded\n");
	}
	DBG(4, "  layed_out = %d\n", ti->l.layed_out);
	DBG(4, "  update_pos = %d\n", ti->l.update_pos);
	DBG(4, "  grd_rect = (%d,%d) %dx%d\n", 
			ti->l.grd_rect.x, ti->l.grd_rect.y,
			ti->l.grd_rect.w, ti->l.grd_rect.h);
	DBG(4, "  icn_rect = (%d,%d) %dx%d\n", 
			ti->l.icn_rect.x, ti->l.icn_rect.y,
			ti->l.icn_rect.w, ti->l.icn_rect.h);
	DBG(4, "  wnd_sz = (%d, %d)\n", ti->l.wnd_sz.x, ti->l.wnd_sz.y);
	DBG(4, "  cmode = %d\n", ti->cmode);

	return NO_MATCH;
}
#endif

