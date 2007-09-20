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

#include "common.h"
#include "debug.h"
#include "layout.h"
#include "list.h"
#include "tray.h"

#ifdef DEBUG
#include "xutils.h"
#endif

struct TrayIcon *icons_head = NULL;

struct TrayIcon *add_icon(Window w, int cmode)
{
	struct TrayIcon *new_icon;

	if (find_icon(w) != NULL)
		return NULL;
	
	if ((new_icon = (struct TrayIcon *) malloc(sizeof(struct TrayIcon))) == NULL)
		DIE(("Out of memory\n"));

	new_icon->w = w;
	new_icon->l.layed_out = False;
	new_icon->l.wnd_sz.x = 0;
	new_icon->l.wnd_sz.y = 0;
	new_icon->embedded = False;
	new_icon->l.p = 0;
	new_icon->l.update_pos = False;
	new_icon->l.resized = True;
	new_icon->cmode = cmode;
	new_icon->invalid = True;

	LIST_ADD_ITEM(icons_head, new_icon);
	return new_icon;
}

int del_icon(struct TrayIcon *ti)
{
	if (ti != NULL) {
		LIST_DEL_ITEM(icons_head, ti);
		free(ti);
	}
	return SUCCESS;
}

struct TrayIcon *next_icon(struct TrayIcon *ti)
{
	return (ti != NULL && ti->next != NULL) ? ti->next : icons_head;
}

struct TrayIcon *prev_icon(struct TrayIcon *ti)
{
	struct TrayIcon *tmp;
	if (ti != NULL && ti->prev != NULL) 
		return ti->prev;
	else {
		tmp = icons_head;
		for (; tmp->next != NULL; tmp = tmp->next);
		return tmp;
	}
}

static struct TrayIcon *backup_head = NULL;

int icons_backup()
{
	struct TrayIcon *tmp, *cur, *cur2;

	if (backup_head != NULL) {
		DBG(0, ("double backup is unsupported\n"));
		return FAILURE;
	}

	for (cur = icons_head, cur2 = NULL; cur != NULL; cur = cur->next, cur2 = tmp) {
		if ((tmp = (struct TrayIcon *) malloc(sizeof(struct TrayIcon))) == NULL)
			DIE(("Out of memory\n"));

		memcpy(tmp, cur, sizeof(struct TrayIcon));
		LIST_INSERT_AFTER(backup_head, cur2, tmp);
		cur2 = tmp;
	}
	return SUCCESS;
}

int icons_restore()
{
	struct TrayIcon *cur_b, *cur_i, *prev_sv, *next_sv;
	DBG(6, ("restoring the icon list from the backup\n"));
	/* this assumes that sequences have the same length */
	for (cur_i = icons_head, cur_b = backup_head;
	     cur_i != NULL && cur_b != NULL;
		 cur_i = cur_i->next, cur_b = cur_b->next)
	{
		prev_sv = cur_i->prev; next_sv = cur_i->next;
		memcpy(cur_i, cur_b, sizeof(struct TrayIcon));
		cur_i->prev = prev_sv; cur_i->next = next_sv;
	}
	if (cur_i != NULL || cur_b != NULL) {
		DBG(0, ("internal error\n"));
		return FAILURE;
	}
	LIST_CLEAN(backup_head, cur_b);
	backup_head = NULL;
	return SUCCESS;
}

int icons_purge_backup()
{
	struct TrayIcon *tmp;
	DBG(6, ("purging the backed up icon list\n"));
	LIST_CLEAN(backup_head, tmp);
	backup_head = NULL;
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

struct TrayIcon *find_icon_ex(Window w)
{
	struct TrayIcon *tmp;
	for (tmp = icons_head; tmp != NULL; tmp = tmp->next)
		if (tmp->w == w || tmp->l.p == w)
			return tmp;

	return NULL;
}

int clean_icons(IconCallbackFunc cbk)
{
	struct TrayIcon *tmp;
	LIST_CLEAN_CBK(icons_head, tmp, cbk);
	return SUCCESS;
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
}

struct TrayIcon *forall_icons(IconCallbackFunc cbk)
{
	return forall_icons_from(icons_head, cbk);
}


struct TrayIcon *forall_icons_from(struct TrayIcon *tgt, IconCallbackFunc cbk)
{
	struct TrayIcon *tmp;
	for (tmp = tgt != NULL ? tgt : icons_head; tmp != NULL; tmp = tmp->next) 
		if (cbk(tmp) == MATCH) {
			return tmp;
		}

	return NULL;
}

#ifdef DEBUG
int print_icon_data(struct TrayIcon *ti)
{
	XWindowAttributes xwa;
	Window real_parent, root, *children = NULL;
	unsigned int nchildren = 0;

	DBG(4, ("ptr = %p\n", ti));
	DBG(4, ("  prev = %p\n", ti->prev));
	DBG(4, ("  next = %p\n", ti->next));
	DBG(4, ("  wid = 0x%x\n", ti->w));
	DBG(4, ("  invalid = %d\n", ti->invalid));
	DBG(4, ("  xembed_supported = %d\n", ti->xembed_supported));
	DBG(4, ("  xembed_wants_focus = %d\n", ti->xembed_wants_focus));
	DBG(4, ("  xembed_last_timestamp = %d\n", ti->xembed_last_timestamp));
	DBG(4, ("  xembed_last_msgid = %d\n", ti->xembed_last_msgid));

	trap_errors();
	
	XQueryTree(tray_data.dpy, ti->w, &root, &real_parent, &children, &nchildren);

	if (children != NULL && nchildren > 0) XFree(children);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("could not get real mid-parent\n"));
	} else {
		DBG(4, ("  real parent wid: 0x%x\n", real_parent));
	}
	
	if (ti->embedded) {
		trap_errors();
	
		XGetWindowAttributes(tray_data.dpy, ti->l.p, &xwa);
	
		if (untrap_errors(tray_data.dpy)) {
			DBG(4, ("  WEIRD: could not get mid-parents stats (%d)\n", trapped_error_code));
		} else {
			DBG(4, ("  mid-parent wid:      0x%x\n", ti->l.p));
			DBG(4, ("  mid-parent state:    %s\n",  xwa.map_state == IsUnmapped ? "Unmapped" : (xwa.map_state == IsUnviewable ? "Unviewable" : "Viewable")));
			DBG(4, ("  mid-parent geometry: %dx%d+%d+%d\n", xwa.width, xwa.height, xwa.x, xwa.y));
		}
	} else {
		DBG(4, ("  not embedded\n"));
	}
	DBG(4, ("  layed_out = %d\n", ti->l.layed_out));
	DBG(4, ("  update_pos = %d\n", ti->l.update_pos));
	DBG(4, ("  grd_rect = (%d,%d) %dx%d\n", 
			ti->l.grd_rect.x, ti->l.grd_rect.y,
			ti->l.grd_rect.w, ti->l.grd_rect.h));
	DBG(4, ("  icn_rect = (%d,%d) %dx%d\n", 
			ti->l.icn_rect.x, ti->l.icn_rect.y,
			ti->l.icn_rect.w, ti->l.icn_rect.h));
	DBG(4, ("  wnd_sz = (%d, %d)\n", ti->l.wnd_sz.x, ti->l.wnd_sz.y));
	DBG(4, ("  cmode = %d\n", ti->cmode));

	trap_errors();

	XGetWindowAttributes(tray_data.dpy, ti->w, &xwa);
	
	if (untrap_errors(tray_data.dpy)) {
		DBG(0, ("could not get icons window attributes\n"));
	} else {
		DBG(4, ("  geometry: %dx%d+%d+%d\n", xwa.width, xwa.height, xwa.x, xwa.y));
		DBG(4, ("  mapstate: %s\n", xwa.map_state == IsUnmapped ? "Unmapped" : (xwa.map_state == IsUnviewable ? "Unviewable" : "Viewable")));
	}

	return NO_MATCH;
}
#else
int print_icon_data(struct TrayIcon *ti)
{
	fprintf(stderr, "ptr = %p\n", ti);
	fprintf(stderr, "\tprev = %p\n", ti->prev);
	fprintf(stderr, "\tnext = %p\n\n", ti->next);
	fprintf(stderr, "\twid = 0x%x\n\n", ti->w);
	fprintf(stderr, "\tinvalid = %d\n", ti->invalid);
	fprintf(stderr, "\tsupports_xembed = %d\n", ti->supports_xembed);
	fprintf(stderr, "\tlayed_out = %d\n", ti->l.layed_out);
	fprintf(stderr, "\tupdate_pos = %d\n", ti->l.update_pos);
	fprintf(stderr, "\tcmode = %d\n\n", ti->cmode);
	fprintf(stderr, "\tgrd_rect = %dx%d+%d+%d\n",
		ti->l.grd_rect.w, ti->l.grd_rect.h,
		ti->l.grd_rect.x, ti->l.grd_rect.y);
	fprintf(stderr, "\ticn_rect = %dx%d+%d+%d\n\n",
		ti->l.icn_rect.w, ti->l.icn_rect.h,
		ti->l.icn_rect.x, ti->l.icn_rect.y);

	return NO_MATCH;
}
#endif

