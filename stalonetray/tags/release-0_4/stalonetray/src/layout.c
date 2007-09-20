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

#include "layout.h"

#include "common.h"
#include "config.h"
#include "debug.h"
#include "icons.h"
#include "tray.h"
#include "list.h"
#include "embed.h"
#include "xembed.h"
#include "xutils.h"

#define CMP(a,b) ((a < b) ? -1 : ((a > b) ? 1 : 0))

#define UPD(tgt,val,flag) {flag=(tgt!=val)||flag;tgt=val;}

#define WND2GRID(l) {\
	(l).grd_rect.w = (l).wnd_sz.x / settings.icon_size + ((l).wnd_sz.x % settings.icon_size != 0); \
	(l).grd_rect.h = (l).wnd_sz.y / settings.icon_size + ((l).wnd_sz.y % settings.icon_size != 0); \
}

#define RECT_ISECT_(r1, r2)	(((RX1(r1) <= RX1(r2) && RX1(r2) <= RX2(r1)) || \
                              (RX1(r1) <= RX2(r2) && RX2(r2) <= RX2(r1))) && \
                             ((RY1(r1) <= RY1(r2) && RY1(r2) <= RY2(r1)) || \
                              (RY1(r1) <= RY2(r2) && RY2(r2) <= RY2(r1))))

#define RECT_ISECT(r1, r2) (RECT_ISECT_(r1,r2) || RECT_ISECT_(r2,r1))

struct IconPlacement {
	struct IconPlacement *next, *prev;
	struct Point pos, ggrow, grow;
};

struct Point grid_sz = {0, 0};

int add_icon_to_grid(struct TrayIcon *ti);
int remove_icon_from_grid(struct TrayIcon *ti);

int update_grid(struct TrayIcon *ti);

int trayicon_cmp_func(struct TrayIcon *ti1, struct TrayIcon *ti2);

int grid2window(struct TrayIcon *ti);

int layout_icon(struct TrayIcon *ti)
{
	struct Point icn_sz = {FALLBACK_SIZE, FALLBACK_SIZE};
	if (ti->cmode == CM_KDE) {
		icn_sz.x = 22;
		icn_sz.y = 22;
	}

	if (set_window_size(ti, icn_sz) && add_icon_to_grid(ti)) {
		forall_icons(&grid2window);
		sort_icons(&trayicon_cmp_func);
	}
	
	return ti->l.layed_out;
}

int unlayout_icon(struct TrayIcon *ti)
{
	if (ti->l.layed_out) {
		ti->l.layed_out = 0;
		remove_icon_from_grid(ti);
	}

	return SUCCESS;
}

int layout_handle_icon_resize(struct TrayIcon *ti)
{
	struct Layout l;

#if 0 /* I hope it works now =) */
	DBG(0, ("NOT TESTED (== BUGGY)\n"));
#endif

	/* in this case caller should consider
	 * removing this icon from the layout */
	if (!get_window_size(ti, &l.wnd_sz))
		return FAILURE;

#ifdef DEBUG
	forall_icons(&print_icon_data);
#endif

	if (ti->l.layed_out) {
		DBG(4, ("the icon is already layed out. updating\n"));
		WND2GRID(l);
	
		if (l.wnd_sz.x != ti->l.wnd_sz.x || l.wnd_sz.y != ti->l.wnd_sz.y) {
			ti->l.wnd_sz = l.wnd_sz;
			ti->l.resized = True;
		} else 
			DBG(4, ("size did not change\n"));

		if (l.grd_rect.w != ti->l.grd_rect.w || l.grd_rect.h != ti->l.grd_rect.h)
			update_grid(ti);
	} else {
		DBG(4, ("the icon needs to be added to the grid\n"));
		ti->l.wnd_sz = l.wnd_sz;
		ti->l.resized = True;

		icons_backup();
		if (!add_icon_to_grid(ti)) {
			icons_restore();
			return FAILURE;
		}
		icons_purge_backup();
		
		forall_icons(&grid2window);
		sort_icons(&trayicon_cmp_func);
	}
	
#ifdef DEBUG
	forall_icons(&print_icon_data);
#endif

	return SUCCESS;
}

void layout_get_size(unsigned int *width, unsigned int *height)
{
	*width = grid_sz.x * settings.icon_size;
	*height = grid_sz.y * settings.icon_size;
}

/**************************** 
 * the implementation stuff * 
 ****************************/

struct IconPlacement *find_placement(struct Layout *l);
int update_grid_pos(struct TrayIcon *ti, struct Point *p);
int update_icon_grid_placement(struct TrayIcon *ti);
int recalc_grid_size(struct TrayIcon *ti);
int unset_layed_out_flag(struct TrayIcon *ti);
int add_icon_to_grid_wrapper(struct TrayIcon *ti);

int add_icon_to_grid(struct TrayIcon *ti)
{
	int retval = FAILURE;
	struct IconPlacement *pmt;
	struct Layout *l = &ti->l;

	WND2GRID(ti->l);

	pmt = find_placement(l);
	
	if (pmt == NULL) {
		DBG(0, ("no candidates for placement found\n"));
		return retval;
	}

	update_grid_pos(ti, &pmt->pos);

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
	ti->l.layed_out = retval;	
	return retval;
}

int remove_icon_from_grid(struct TrayIcon *ti)
{
	forall_icons_from(ti->next, &update_icon_grid_placement);	
	sort_icons(&trayicon_cmp_func);
	forall_icons(&grid2window);

	grid_sz.x = 0;
	grid_sz.y = 0;
	forall_icons(&recalc_grid_size);

	return SUCCESS;
}

int update_grid(struct TrayIcon *ti)
{
	forall_icons_from(ti, &unset_layed_out_flag);
	
	forall_icons_from(ti, &add_icon_to_grid_wrapper);
	forall_icons(&grid2window);
	sort_icons(&trayicon_cmp_func);

	grid_sz.x = 0;
	grid_sz.y = 0;
	forall_icons(&recalc_grid_size);

	return SUCCESS;
}

int trayicon_cmp_func(struct TrayIcon *ti1, struct TrayIcon *ti2)
{
	int cx, cy;

	cx = CMP(ti1->l.grd_rect.x, ti2->l.grd_rect.x);
	cy = CMP(ti1->l.grd_rect.y, ti2->l.grd_rect.y);
	
	if (settings.vertical) {
		return cx == 0 ? cy : cx;
	} else {
		return cy == 0 ? cx : cy;
	}
}

int grid2window(struct TrayIcon *ti)
{
	
	struct Rect *in = &ti->l.grd_rect;
	struct Rect *out = &ti->l.icn_rect;

	if (!ti->l.layed_out) return NO_MATCH;
	
	if (settings.gravity & GRAV_W) {
		UPD(out->x, 
			in->x * settings.icon_size, 
			ti->l.update_pos);
	} else {
		UPD(out->x, 
			tray_data.xsh.width - (in->x + in->w) * settings.icon_size, 
			ti->l.update_pos);
	}
	UPD(out->w, in->w * settings.icon_size, ti->l.update_pos);
	
	if (settings.gravity & GRAV_N) {
		UPD(out->y, 
			in->y * settings.icon_size, 
			ti->l.update_pos);
	} else {
		UPD(out->y,
			tray_data.xsh.height - (in->y + in->h) * settings.icon_size, 
			ti->l.update_pos);
	}
	UPD(out->h, in->h * settings.icon_size, ti->l.update_pos);

	return NO_MATCH;
}

/*************************
 * More low-level stuff  *
 *************************/

int find_obstacle(struct TrayIcon *ti);
int grid_check_rect_free(int x, int y, int w, int h);
int ip_leq(struct IconPlacement *ip1, struct IconPlacement *ip2);
int new_icon_placement(struct IconPlacement **ip_head, int x, int y, struct Layout *l);

static struct Rect chk_rect;

int find_obstacle(struct TrayIcon *ti)
{
	return ti->l.layed_out && 
		(RECT_ISECT(chk_rect, ti->l.grd_rect) || RECT_ISECT(ti->l.grd_rect, chk_rect));
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

int ip_leq(struct IconPlacement *ip1, struct IconPlacement *ip2)
{
	int rc = CMP(ip1->grow.x + ip1->grow.y, ip2->grow.x + ip2->grow.y);
	int gr = ip1->grow.x + ip1->grow.y > 0 && ip2->grow.x + ip2->grow.y > 0;
	int rc_x, rc_y;
	int ret;

	if (settings.min_space_policy) {
 		ret = rc <= 0;
	} else {
		rc_x = CMP(ip1->pos.x, ip2->pos.x);
		rc_y = CMP(ip1->pos.y, ip2->pos.y);
		
		if (settings.vertical) {
			ret = rc_x < 0 ? 1 : (rc_x == 0 ? rc_y <= 0 : 0);
		} else {
			ret = rc_y < 0 ? 1 : (rc_y == 0 ? rc_x <= 0 : 0);
		}
		
		ret = gr ? ret : (rc < 0 ? 1 : 0);
	}

	DBG(8, ("(%d, %d, %d, %d) %s (%d, %d, %d, %d) | rc=%d, rc_x=%d, rc_y=%d, ret=%d\n",
			ip1->pos.x, ip1->pos.y, ip1->grow.x, ip1->grow.y,
			ret ? "<=" : ">",
			ip2->pos.x, ip2->pos.y, ip2->grow.x, ip2->grow.y,
			rc, rc_x, rc_y, ret));

	return ret;
}

int new_icon_placement(struct IconPlacement **ip_head, int x, int y, struct Layout *l) 
{
	struct IconPlacement *ip, *tmp, *tmp_prev;
	struct Point ggrow, grow;

	ggrow.x = x + l->grd_rect.w - grid_sz.x;
	ggrow.y = y + l->grd_rect.h - grid_sz.y;

	ggrow.x = ggrow.x > 0 ? ggrow.x : 0;
	ggrow.y = ggrow.y > 0 ? ggrow.y : 0;
	
	grow.x = (ggrow.x + grid_sz.x) * settings.icon_size - tray_data.xsh.width;
	grow.y = (ggrow.y + grid_sz.y) * settings.icon_size - tray_data.xsh.height;

	grow.x = grow.x > 0 ? grow.x : 0;
	grow.y = grow.y > 0 ? grow.y : 0;
	
	if (!tray_grow_check(grow)) {
		DBG(6, ("placement (%d, %d, %d, %d) violates grow settings\n", x, y, grow.x, grow.y));
		
		if (settings.full_pmt_search) {
			return True;
		} else {
			return False;
		}
	}

	ip = (struct IconPlacement *) malloc(sizeof(struct IconPlacement));

	if (ip == NULL) {
		DIE(("out of memory\n"));
	}

	ip->pos.x = x;
	ip->pos.y = y;

	ip->grow = grow;
	ip->ggrow = ggrow;

	for (tmp = *ip_head, tmp_prev = NULL; tmp != NULL && ip_leq(tmp, ip); tmp_prev = tmp, tmp = tmp->next);
	LIST_INSERT_AFTER((*ip_head), tmp_prev, ip);

#ifdef DEBUG
	for (tmp = *ip_head; tmp != NULL; tmp = tmp->next) {
		DBG(6, ("  pmt: (%d, %d, %d, %d)\n", tmp->pos.x, tmp->pos.y, tmp->grow.x, tmp->grow.y)); 
	}
#endif

	DBG(4, ("new placement (%d, %d, %d, %d)\n", x, y, grow.x, grow.y));

	return grow.x || grow.y;
}

/* returns ptr to STATIC buffer */
struct IconPlacement *find_placement(struct Layout *l)
{
	static struct IconPlacement result;
	struct IconPlacement *tmp, *ip_head = NULL;
	int x = 0, y = 0, skip, orig_layed_out;

	orig_layed_out = l->layed_out;
	l->layed_out = 0; /* to avoid self-intersections */
	
	if (grid_sz.x == 0 || grid_sz.x == 0) { 	/* grid is empty */
		new_icon_placement(&ip_head, 0, 0, l);
	} else {									/* seek and place */
		new_icon_placement(&ip_head, settings.vertical ? grid_sz.x : 0, 
				settings.vertical ? 0 : grid_sz.y, l);
		new_icon_placement(&ip_head, settings.vertical ? 0: grid_sz.x, 
				settings.vertical ? grid_sz.y : 0, l);
		do {
			skip = grid_check_rect_free(x, y, l->grd_rect.w, l->grd_rect.h);

			DBG(8, ("x=%d, y=%d, skip=%d\n", x, y, skip));

			if (skip == 0) {
				if (!new_icon_placement(&ip_head, x, y, l)) break;
				skip = settings.vertical ? l->grd_rect.w : l->grd_rect.h;
			}
			
			if (settings.vertical) {
				y += skip;
				if (y >= grid_sz.y) {
					y = 0;
					x++;
				}
			} else {
				x += skip;
				if (x >= grid_sz.x) {
					x = 0;
					y++;
				}
			}
		} while (x < grid_sz.x && y < grid_sz.y);
	}

	l->layed_out = orig_layed_out;

	if (ip_head != NULL) {
#ifdef DEBUG
		for (tmp = ip_head; tmp != NULL; tmp = tmp->next) {
			DBG(8, ("pmt: (%d, %d, %d, %d)\n", tmp->pos.x, tmp->pos.y, tmp->grow.x, tmp->grow.y));
		}
#endif
		
		result = *ip_head;

		LIST_CLEAN(ip_head, tmp);

		DBG(4, ("using placement (%d, %d, %d, %d)\n", 
				result.pos.x, result.pos.y, 
				result.grow.x, result.grow.y));
		
		return &result;
	} else {
		return NULL;
	}
}

int update_grid_pos(struct TrayIcon *ti, struct Point *p)
{
	struct Layout *l = &ti->l;
	
	l->update_pos = p->x != l->grd_rect.x || p->y != l->grd_rect.y;
	if (l->update_pos) {
		DBG(4, ("updating pos for (%d,%d) to (%d,%d)\n", 
				ti->l.grd_rect.x, ti->l.grd_rect.y,
				p->x, p->y));
		l->grd_rect.x = p->x;
		l->grd_rect.y = p->y;
	}
	
	grid2window(ti);
	
	return l->update_pos;
}

int update_icon_grid_placement(struct TrayIcon *ti)
{
	struct IconPlacement *icn_pmt = NULL;
	
	icn_pmt = find_placement(&ti->l);

	DBG(4, ("searching for placement for (%d,%d)\n",
			ti->l.grd_rect.x, ti->l.grd_rect.y));
	
	if (icn_pmt != NULL && icn_pmt->grow.x == 0 && icn_pmt->grow.y == 0) {
		DBG(6, ("placement: (%d,%d)\n", icn_pmt->pos.x, icn_pmt->pos.y));
		if (!update_grid_pos(ti, &icn_pmt->pos) && 	settings.minimal_movement) {
			DBG(4, ("success\n"));
			return SUCCESS;
		}
	}

	DBG(4, ("failure\n"));
	return FAILURE;
}

int recalc_grid_size(struct TrayIcon *ti)
{
	int x, y;
	
	if (!ti->l.layed_out) return NO_MATCH;
	
	x = ti->l.grd_rect.x + ti->l.grd_rect.w;
	y = ti->l.grd_rect.y + ti->l.grd_rect.h;
	
	grid_sz.x = x > grid_sz.x ? x : grid_sz.x;
	grid_sz.y = y > grid_sz.y ? y : grid_sz.y;
	DBG(4, ("new grid size: %dx%d\n", grid_sz.x, grid_sz.y));

	return NO_MATCH;
}

int unset_layed_out_flag(struct TrayIcon *ti)
{
	ti->l.layed_out = 0;
	return NO_MATCH;
}

int add_icon_to_grid_wrapper(struct TrayIcon *ti)
{
	return !add_icon_to_grid(ti);
}

