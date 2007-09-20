/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * layout.c
 * Tue, 24 Aug 2004 12:19:48 +0700
 * -------------------------------
 * Icon layout implementation.
 * The dirtiest place around.
 * -------------------------------*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "config.h"

#include "common.h"

#include "layout.h"
#include "debug.h"
#include "icons.h"
#include "tray.h"
#include "list.h"

/* not very nice, but allows doing things like swap(*a, *b) */
#define swap(a,b) do { int t; t = (a); (a) = (b); (b) = t; } while (0);

/***************** 
* Grid interface *
*****************/

struct Point grid_sz = {0, 0};

int add_icon_to_grid(struct TrayIcon *ti);
int add_icon_to_grid_wrapper(struct TrayIcon *ti);
int remove_icon_from_grid(struct TrayIcon *ti);

int update_grid(struct TrayIcon *ti);
int rebuild_grid(struct TrayIcon *ti);

int window2grid(struct TrayIcon *ti);
int grid2window(struct TrayIcon *ti);

/************************
* Layout implementation *
************************/

int layout_icon(struct TrayIcon *ti)
{	
	if (add_icon_to_grid(ti)) 
		update_grid(ti);

	return ti->is_layed_out;
}

int unlayout_icon(struct TrayIcon *ti)
{
	if (ti->is_layed_out) {
		ti->is_layed_out = 0;
		remove_icon_from_grid(ti);
	}
	return SUCCESS;
}

int layout_handle_icon_resize(struct TrayIcon *ti)
{
	struct Rect old_grd_rect;
	int rc;
#ifdef DEBUG
	forall_icons(&print_icon_data);
#endif

	if (ti->is_layed_out) {
		/* if the icon is already layed up and
		 * its grid rect did not change we do nothing */
		old_grd_rect = ti->l.grd_rect;
		window2grid(ti);
		if (ti->l.grd_rect.w == old_grd_rect.w &&
		    ti->l.grd_rect.h == old_grd_rect.h)
		{
			return SUCCESS;
		}
	}

	icons_backup();
	if (forall_icons_from(ti, &add_icon_to_grid_wrapper) == NULL) {
		icons_purge_backup();
		update_grid(ti);
		rc = SUCCESS;
	} else {
		icons_restore();
		rc = FAILURE;
	}
#ifdef DEBUG
	forall_icons(&print_icon_data);
#endif
	return rc;
}

void layout_get_size(unsigned int *width, unsigned int *height)
{
	*width = grid_sz.x * settings.icon_size;
	*height = grid_sz.y * settings.icon_size;
	if (settings.vertical) swap((*width), (*height));
}

/**********************
* Grid implementation *
**********************/

struct IconPlacement {
	struct Point pos, ggrow, grow;
	int valid;
};

struct IconPlacement *find_placement(struct TrayIcon *ti);
int place_icon(struct TrayIcon *ti, struct IconPlacement *ip);

/* Mass-ops helpers declarations */
int trayicon_cmp_func(struct TrayIcon *ti1, struct TrayIcon *ti2);
int update_icon_grid_placement(struct TrayIcon *ti);
int recalc_grid_size(struct TrayIcon *ti);
int unset_layed_out_flag(struct TrayIcon *ti);

/* A. Coords translations */

int window2grid(struct TrayIcon *ti)
{
	ti->l.grd_rect.w = ti->l.wnd_sz.x / settings.icon_size + (ti->l.wnd_sz.x % settings.icon_size != 0); \
	ti->l.grd_rect.h = ti->l.wnd_sz.y / settings.icon_size + (ti->l.wnd_sz.y % settings.icon_size != 0); \
	if (settings.vertical) swap(ti->l.grd_rect.w, ti->l.grd_rect.h);
	return NO_MATCH;
}

#define UPD(tgt,val,flag) {flag=(tgt!=val)||flag;tgt=val;}

int grid2window(struct TrayIcon *ti)
{
	struct Rect *in = &ti->l.grd_rect;
	struct Rect *out = &ti->l.icn_rect;

	if (!ti->is_layed_out) return NO_MATCH;

	if (settings.vertical) swap(in->w, in->h);
	
	if (settings.icon_gravity & GRAV_W) {
		UPD(out->x, 
			in->x * settings.icon_size, 
			ti->is_updated);
	} else {
		UPD(out->x, 
			tray_data.xsh.width - (in->x + in->w) * settings.icon_size, 
			ti->is_updated);
	}
	UPD(out->w, in->w * settings.icon_size, ti->is_updated);
	
	if (settings.icon_gravity & GRAV_N) {
		UPD(out->y, 
			in->y * settings.icon_size, 
			ti->is_updated);
	} else {
		UPD(out->y,
			tray_data.xsh.height - (in->y + in->h) * settings.icon_size, 
			ti->is_updated);
	}
	UPD(out->h, in->h * settings.icon_size, ti->is_updated);

	if (settings.vertical) swap(in->w, in->h);
	
	return NO_MATCH;
}

/* B. Adding/removing icon from the grid */

int add_icon_to_grid(struct TrayIcon *ti)
{
	int retval = FAILURE;
	struct IconPlacement *pmt;

	window2grid(ti);

	pmt = find_placement(ti);
	
	if (pmt == NULL) {
		DBG(0, ("no candidates for placement found\n"));
		return retval;
	}

	place_icon(ti, pmt);

	if (pmt->grow.x || pmt->grow.y) {
		retval = tray_grow(pmt->grow);
	} else {
		retval = SUCCESS;
	}

	if (retval) {
		grid_sz.x += pmt->ggrow.x;
		grid_sz.y += pmt->ggrow.y;
	}

	DBG(3, (retval ? "success\n" : "failed\n"));
	ti->is_layed_out = retval;	
	return retval;
}

int remove_icon_from_grid(struct TrayIcon *ti)
{
	if (ti->next != NULL) {	
/*		forall_icons_from(ti->next, &unset_layed_out_flag);*/
		forall_icons_from(ti->next, &update_icon_grid_placement);
	}
	update_grid(ti);
	return SUCCESS;
}

/* C. Grid manipulations */

int update_grid(struct TrayIcon *ti)
{
	forall_icons_from(ti, &grid2window);
	sort_icons(&trayicon_cmp_func);
	grid_sz.x = 0;
	grid_sz.y = 0;
	forall_icons(&recalc_grid_size);
	return SUCCESS;
}

/* D. Placement functions */

int place_icon(struct TrayIcon *ti, struct IconPlacement *ip)
{
	struct Layout *l = &ti->l;
	struct Point *p = &ip->pos;
	
	ti->is_updated = p->x != l->grd_rect.x || p->y != l->grd_rect.y;
	if (ti->is_updated) {
		DBG(4, ("updating pos for (%d,%d) to (%d,%d)\n", 
				ti->l.grd_rect.x, ti->l.grd_rect.y,
				p->x, p->y));
		l->grd_rect.x = p->x;
		l->grd_rect.y = p->y;
	}
	
	grid2window(ti);
	
	return ti->is_updated;
}

static struct Rect chk_rect;

#define RECTS_ISECT_(r1, r2)	(((RX1(r1) <= RX1(r2) && RX1(r2) <= RX2(r1)) || \
                              (RX1(r1) <= RX2(r2) && RX2(r2) <= RX2(r1))) && \
                             ((RY1(r1) <= RY1(r2) && RY1(r2) <= RY2(r1)) || \
                              (RY1(r1) <= RY2(r2) && RY2(r2) <= RY2(r1))))

#define RECTS_ISECT(r1, r2) (RECTS_ISECT_(r1,r2) || RECTS_ISECT_(r2,r1))

int find_obstacle(struct TrayIcon *ti)
{
	return ti->is_layed_out && 
		(RECTS_ISECT(chk_rect, ti->l.grd_rect) || RECTS_ISECT(ti->l.grd_rect, chk_rect));
}

int grid_check_rect_free(int x, int y, int w, int h)
{
	struct TrayIcon *obst;

	chk_rect.x = x;
	chk_rect.y = y;
	chk_rect.w = w;
	chk_rect.h = h;
	
	obst = forall_icons(&find_obstacle);

	if (obst == NULL) {
		return 0;
	} else {
		if (settings.vertical) 
			return obst->l.grd_rect.h;
		else
			return obst->l.grd_rect.w;
	}
}

int create_icon_placement(struct IconPlacement *ip, int x, int y, struct Rect *grd_rect)
{
	ip->valid = 0;

	ip->pos.x = x;
	ip->pos.y = y;

	ip->ggrow.x = x + grd_rect->w - grid_sz.x;
	ip->ggrow.y = y + grd_rect->h - grid_sz.y;

	ip->ggrow.x = ip->ggrow.x > 0 ? ip->ggrow.x : 0;
	ip->ggrow.y = ip->ggrow.y > 0 ? ip->ggrow.y : 0;
	
	ip->grow.x = (ip->ggrow.x + grid_sz.x) * settings.icon_size - tray_data.xsh.width;
	ip->grow.y = (ip->ggrow.y + grid_sz.y) * settings.icon_size - tray_data.xsh.height;

	ip->grow.x = ip->grow.x > 0 ? ip->grow.x : 0;
	ip->grow.y = ip->grow.y > 0 ? ip->grow.y : 0;

	ip->valid = tray_grow_check(ip->grow);

#ifdef DEBUG
	DBG(6, ("placement (%d, %d, %d, %d) is %s\n", 
	        x, y, ip->grow.x, ip->grow.y,
	        ip->valid ? "valid" : "invalid"));
#endif

	return ip->valid;
}

#define compare_points(a,b) (((a).y < (b).y) || ((a).y == (b).y && (a).x < (b).x))

void choose_best_placement(struct IconPlacement *old, struct IconPlacement *new)
{
	/* here lives the placement policy */
	int old_grow, new_grow, grow_cmp;

	DBG(6, ("old=(%d, %d, %d, %d), new=(%d, %d, %d, %d)\n",
	        old->pos.x, old->pos.y, old->grow.x, old->grow.y,
	        new->pos.x, new->pos.y, new->grow.x, new->grow.y));

	old_grow = old->grow.x + old->grow.y;
	new_grow = new->grow.x + new->grow.y;
	
	grow_cmp = new_grow < old_grow;

	/* XXX: optimize in one expression? */
	if (settings.min_space_policy && grow_cmp) goto replace;
	if (old_grow && !new_grow) goto replace;
	if ((!old_grow == !new_grow) && compare_points(new->pos, old->pos)) goto replace;

	DBG(6, ("old is better\n"));

	return;

replace:
	DBG(6, ("new is better\n"));
	*old = *new;
}

/* returns ptr to STATIC buffer */
struct IconPlacement *find_placement(struct TrayIcon *ti)
{
	static struct IconPlacement cur_pmt;
	struct IconPlacement tmp_pmt;
	int x = 0, y = 0, skip, orig_layed_out;

	orig_layed_out = ti->is_layed_out;
	ti->is_layed_out = 0; /* to avoid self-intersections */
	
	if (grid_sz.x == 0 || grid_sz.x == 0) { 	/* grid is empty */
		create_icon_placement(&cur_pmt, 0, 0, &ti->l.grd_rect);
	} else {									/* seek and place */
		create_icon_placement(&cur_pmt, grid_sz.x, 0, &ti->l.grd_rect);
		create_icon_placement(&tmp_pmt, 0, grid_sz.y, &ti->l.grd_rect);
		choose_best_placement(&cur_pmt, &tmp_pmt);
		do {
			skip = grid_check_rect_free(x, y, ti->l.grd_rect.w, ti->l.grd_rect.h);
			DBG(8, ("x=%d, y=%d, skip=%d\n", x, y, skip));
			if (skip == 0) {
				if (create_icon_placement(&tmp_pmt, x, y, &ti->l.grd_rect)) {
					choose_best_placement(&cur_pmt, &tmp_pmt);
					if (!settings.full_pmt_search && !cur_pmt.ggrow.x && !cur_pmt.ggrow.y) break;
				}
				skip = ti->l.grd_rect.w;
			}
			x += skip;
			if (x >= grid_sz.x) {
				x = 0;
				y++;
			}
		} while (x < grid_sz.x && y < grid_sz.y);
	}

	ti->is_layed_out = orig_layed_out;

	if (cur_pmt.valid) {
		DBG(4, ("using placement (%d, %d, %d, %d)\n", 
				cur_pmt.pos.x, cur_pmt.pos.y, 
				cur_pmt.grow.x, cur_pmt.grow.y));
		return &cur_pmt;
	} else {
		return NULL;
	}
}


/* E. Mass-ops helpers */

int trayicon_cmp_func(struct TrayIcon *ti1, struct TrayIcon *ti2)
{
	return (!ti2->is_layed_out && ti1->is_layed_out) || 
	       compare_points(ti2->l.grd_rect, ti1->l.grd_rect);
	
#if 0
	if (ti1->l.grd_rect.y > ti2->l.grd_rect.y ||
	    (ti1->l.grd_rect.y == ti2->l.grd_rect.y && 
		 ti1->l.grd_rect.x > ti2->l.grd_rect.x)) return 1;
	
	return 0;
#endif
}

int add_icon_to_grid_wrapper(struct TrayIcon *ti)
{
	int layed_out;
	layed_out = ti->is_layed_out;
	return !layed_out || add_icon_to_grid(ti) ? NO_MATCH : MATCH;
}

int update_icon_grid_placement(struct TrayIcon *ti)
{
	struct IconPlacement *icn_pmt = NULL;
	
	DBG(4, ("searching for placement for (%d,%d)\n",
			ti->l.grd_rect.x, ti->l.grd_rect.y));
	
	icn_pmt = find_placement(ti);

#ifdef DEBUG
	if (icn_pmt == NULL) {
		DBG(6, ("NULL placement\n"));
	} else {
		DBG(6, ("placement: (%d, %d, %d, %d)\n", 
		        icn_pmt->pos.x, icn_pmt->pos.y,
				icn_pmt->grow.x, icn_pmt->grow.y));
	}
#endif

	if (icn_pmt != NULL && icn_pmt->grow.x == 0 && icn_pmt->grow.y == 0) {
		if (place_icon(ti, icn_pmt)) {
			DBG(4, ("success\n"));
			return NO_MATCH;
		}
	}

	DBG(4, ("failure\n"));
	return settings.min_space_policy ? NO_MATCH : MATCH;
}

int recalc_grid_size(struct TrayIcon *ti)
{
	int x, y;
	
	if (!ti->is_layed_out) return NO_MATCH;
	
	x = ti->l.grd_rect.x + ti->l.grd_rect.w;
	y = ti->l.grd_rect.y + ti->l.grd_rect.h;
	
	grid_sz.x = x > grid_sz.x ? x : grid_sz.x;
	grid_sz.y = y > grid_sz.y ? y : grid_sz.y;
	DBG(4, ("new grid size: %dx%d\n", grid_sz.x, grid_sz.y));

	return NO_MATCH;
}

int unset_layed_out_flag(struct TrayIcon *ti)
{
	ti->is_layed_out = 0;
	return NO_MATCH;
}

