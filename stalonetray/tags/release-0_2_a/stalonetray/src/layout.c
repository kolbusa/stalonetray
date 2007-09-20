/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * layout.c
 * Tue, 24 Aug 2004 12:19:48 +0700
 * -------------------------------
 * Icon layout implementation
 * the durtiest place around
 * -------------------------------*/

#include <fnmatch.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "layout.h"

#include "config.h"
#include "debug.h"
#include "icons.h"
#include "tray.h"
#include "list.h"
#include "embed.h"
#include "xembed.h"

/* grid interface */
int add_icon_to_grid(struct TrayIcon *ti);
int remove_icon_from_grid(struct TrayIcon *ti);
int recreate_grid();
int update_grid(struct TrayIcon *ti);
int ti_cmp(struct TrayIcon *ti1, struct TrayIcon *ti2);

/* size-related funcs */
int match_forced_resize_list(struct TrayIcon *ti);
int force_icon_size(struct TrayIcon *ti);
int get_icon_size(struct TrayIcon *ti);
int revalidate_tray_size();
int grid2window(struct TrayIcon *ti);

int layout_icon(struct TrayIcon *ti)
{
	if (ti->cmode == CM_KDE) {
		ti->l.wnd_sz.x = FALLBACK_SIZE;
		ti->l.wnd_sz.y = FALLBACK_SIZE;
	}
	
	if (!match_forced_resize_list(ti) && ti->cmode != CM_KDE && !get_icon_size(ti))
		return 0;

	if (force_icon_size(ti) && add_icon_to_grid(ti)) {
		forall_icons(&grid2window);
		sort_icons(&ti_cmp);
	}

	return ti->l.layed_out;
}

int unlayout_icon(struct TrayIcon *ti)
{
	ti->l.layed_out = 0;
	remove_icon_from_grid(ti);
}

/* prior to this you must freeze grow */
int layout_update()
{
	DBG(0, "NOT TESTED (== BUGGY)\n");
	recreate_grid();	
}

/* prior to this you must freeze grow */
int layout_handle_icon_resize(struct TrayIcon *ti)
{
	DBG(0, "NOT TESTED (== BUGGY)\n");

	ti->l.resized = 1;
	
	/* in this case caller should consider
	 * removing this icon */
	if (!get_icon_size(ti))
		return 0;

	update_grid(ti);
	
	return 1;
}

/******************************************/
/* the implementation stuff               */
/******************************************/

/**** size-related funcs *****/

int match_forced_resize_list(struct TrayIcon *ti)
{
	ForceEntry *fe;
	XClassHint xch;
	int retval = 0;

	XGetClassHint(tray_data.dpy, ti->w, &xch);
	
	/* XXX: sorted search ? */
	for (fe = settings.f_head; fe != NULL; fe = fe->next) {
		if (!fnmatch(fe->pattern, xch.res_name, FNM_CASEFOLD) || 
			!fnmatch(fe->pattern, xch.res_name, FNM_CASEFOLD)) 
		{
			ti->l.wnd_sz.x = fe->width;
			ti->l.wnd_sz.y = fe->height;
			DBG(2, "(%s, %s) matched pattern \"%s\", forcing to (%d, %d)\n", 
					xch.res_name, xch.res_class, fe->pattern, ti->l.wnd_sz.x, ti->l.wnd_sz.y);
			retval = 1;
			break;
		}
	}
	XFree(xch.res_name);
	XFree(xch.res_class);

	return retval;
}

int force_icon_size(struct TrayIcon *ti)
{
	XSizeHints xsh = {
		.flags = PSize,
		.width = ti->l.wnd_sz.x,
		.height = ti->l.wnd_sz.x
	};

	trap_errors();
	
	XSetWMNormalHints(tray_data.dpy, ti->w, &xsh);

	XResizeWindow(tray_data.dpy, ti->w, ti->l.wnd_sz.x, ti->l.wnd_sz.y);

	if (untrap_errors(tray_data.dpy)) {
		DBG(0, "failed to force 0x%x size to (%d, %d) (error code %d)\n", 
				ti->w, ti->l.wnd_sz.x, ti->l.wnd_sz.x, trapped_error_code);
		ti->l.wnd_sz.x = 0;
		ti->l.wnd_sz.y = 0;
		return 0;
	}
	
	return 1;
}

#define NUM_GETSIZE_RETRIES	100

int get_icon_size(struct TrayIcon *ti)
{
	XWindowAttributes xwa;
	int atmpt = 0;

	ti->l.wnd_sz.x = FALLBACK_SIZE;
	ti->l.wnd_sz.y = FALLBACK_SIZE;
	
	for(;;) {
		trap_errors();

		XGetWindowAttributes(tray_data.dpy, ti->w, &xwa);
	
		if (untrap_errors(tray_data.dpy)) {
			DBG(0, "failed to get 0x%x attributes (error code %d)\n", ti->w, trapped_error_code);
			return 0;
		}
	
		ti->l.wnd_sz.x = xwa.width;
		ti->l.wnd_sz.y = xwa.height;

		if (ti->l.wnd_sz.x * ti->l.wnd_sz.y > 1) {
			DBG(2, "success (%dx%d)\n", ti->l.wnd_sz.x, ti->l.wnd_sz.y);
			return 1;
		}

		if (atmpt++ > NUM_GETSIZE_RETRIES) {
			DBG(0, "timeout waiting for 0x%x to specify its size. Using fallback (%d,%d)\n", 
					ti->w, FALLBACK_SIZE, FALLBACK_SIZE);
			return 1;
		}
		usleep(500);
	};
}

int revalidate_tray_size()
{
	/* TODO */
	DBG(0, "NOT IMPLEMENTED\n");
	return 1;
}

/**** G R I D ****/

struct IconPlacement {
	struct IconPlacement *next, *prev;
	struct Point pos, ggrow, grow;
};

struct Point grid_sz = {.x = 0, .y = 0};

/**** GRID AUX  ****/

#define CMP(a,b) ((a < b) ? -1 : ((a > b) ? 1 : 0))
#define UPD(tgt,val,flag) {flag=(tgt!=val)||flag;tgt=val;}

int ti_cmp(struct TrayIcon *ti1, struct TrayIcon *ti2)
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

/**** transation(s) */
int grid2window(struct TrayIcon *ti)
{
	
	struct Rect *in = &ti->l.grd_rect;
	struct Rect *out = &ti->l.icn_rect;
	
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

	return 0;
}

/**** rect functions */

static struct Rect chk_rect;

#define RECT_ISECT(r1, r2)	(((GX1(r1) <= GX1(r2) && GX1(r2) <= GX2(r1)) || \
							  (GX1(r1) <= GX2(r2) && GX2(r2) <= GX2(r1))) && \
							 ((GY1(r1) <= GY1(r2) && GY1(r2) <= GY2(r1)) || \
							  (GY1(r1) <= GY2(r2) && GY2(r2) <= GY2(r1))))

int chk_isect(struct TrayIcon *ti)
{
	return ti->l.layed_out && 
		(RECT_ISECT(chk_rect, ti->l.grd_rect) || RECT_ISECT(ti->l.grd_rect, chk_rect));
}
/* XXX: make use of struct Rect */
int grid_check_rect(int x, int y, int w, int h)
{
	struct TrayIcon *obst;

	chk_rect.x = x;
	chk_rect.y = y;
	chk_rect.w = w;
	chk_rect.h = h;
	
	obst = forall_icons(chk_isect);

	if (obst == NULL) {
		return 0;
	} else {
		if (settings.vertical) 
			return obst->l.grd_rect.h;
		else
			return obst->l.grd_rect.w;
	}
}

/***** placement processing */

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

	DBG(4, "(%d, %d, %d, %d) %s (%d, %d, %d, %d) | rc=%d, rc_x=%d, rc_y=%d, ret=%d\n",
			ip1->pos.x, ip1->pos.y, ip1->grow.x, ip1->grow.y,
			ret ? "<=" : ">",
			ip2->pos.x, ip2->pos.y, ip2->grow.x, ip2->grow.y,
			rc, rc_x, rc_y, ret);

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
		DBG(2, "placement (%d, %d, %d, %d) violates grow settings\n", x, y, grow.x, grow.y);
		
		if (settings.full_pmt_search) {
			return 1;
		} else {
			return 0;
		}
	}

	ip = (struct IconPlacement *) malloc(sizeof(struct IconPlacement));

	if (ip == NULL) {
		DIE("out of memory\n");
	}

	ip->pos.x = x;
	ip->pos.y = y;

	ip->grow = grow;
	ip->ggrow = ggrow;

	for (tmp = *ip_head, tmp_prev = NULL; tmp != NULL && ip_leq(tmp, ip); tmp_prev = tmp, tmp = tmp->next);
	LIST_INSERT_AFTER((*ip_head), tmp_prev, ip);

#ifdef DEBUG
	for (tmp = *ip_head; tmp != NULL; tmp = tmp->next) {
		DBG(4, "pmt: (%d, %d, %d, %d)\n", tmp->pos.x, tmp->pos.y, tmp->grow.x, tmp->grow.y); 
	}
	DBG(4, "---\n");
#endif

	DBG(2, "new placement (%d, %d, %d, %d)\n", x, y, grow.x, grow.y);

	return grow.x || grow.y;
}

/* returns ptr to STATIC buffer */
struct IconPlacement *find_placement(struct Layout *l)
{
	static struct IconPlacement result;
	struct IconPlacement *tmp, *ip_head = NULL;
	int x = 0, y = 0, skip, orig_layed_out;

	orig_layed_out = l->layed_out;
	l->layed_out = 0;
	
	if (grid_sz.x == 0 || grid_sz.x == 0) { 	/* grid is empty */
		new_icon_placement(&ip_head, 0, 0, l);
	} else {									/* seek and place */
		new_icon_placement(&ip_head, settings.vertical ? grid_sz.x : 0, 
				settings.vertical ? 0 : grid_sz.y, l);
		new_icon_placement(&ip_head, settings.vertical ? 0: grid_sz.x, 
				settings.vertical ? grid_sz.y : 0, l);
		do {
			skip = grid_check_rect(x, y, l->grd_rect.w, l->grd_rect.h);

			DBG(3, "x=%d, y=%d, skip=%d\n", x, y, skip);

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
			DBG(4, "pmt: (%d, %d, %d, %d)\n", tmp->pos.x, tmp->pos.y, tmp->grow.x, tmp->grow.y);
		}
#endif
		
		result = *ip_head;
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
		DBG(4, "updating pos for (%d,%d) to (%d,%d)\n", 
				ti->l.grd_rect.x, ti->l.grd_rect.y,
				p->x, p->y);
		l->grd_rect.x = p->x;
		l->grd_rect.y = p->y;
	}
	
	grid2window(ti);
	
	return l->update_pos;
}

/**** GRID CORE ****/
#define WND2GRID(l) \
	(l).grd_rect.w = (l).wnd_sz.x / settings.icon_size + ((l).wnd_sz.x % settings.icon_size != 0); \
	(l).grd_rect.h = (l).wnd_sz.y / settings.icon_size + ((l).wnd_sz.y % settings.icon_size != 0);

int add_icon_to_grid(struct TrayIcon *ti)
{
	int retval = 0;
	struct IconPlacement *pmt;
	struct Layout *l = &ti->l;

	WND2GRID(ti->l);

	/* 1. find a place in a grid that is large enough for the icon */
	pmt = find_placement(l);
	
	/* 2. check the placements and fill layout of "ti"*/
	if (pmt == NULL) {
		DBG(0, "no candidates for placement found\n");
		return 0;
	}

	DBG(2, "using placement (%d, %d, %d, %d)\n", 
			pmt->pos.x, pmt->pos.y, 
			pmt->grow.x, pmt->grow.y);

	update_grid_pos(ti, &pmt->pos);

	/* 3. grow if necessary */
	if (pmt->grow.x || pmt->grow.y) {
		retval = tray_grow(pmt->grow);
	} else {
		retval = 1;
	}

	/* 4. update grid and icon data */
	if (retval) {
		grid_sz.x += pmt->ggrow.x;
		grid_sz.y += pmt->ggrow.y;
	}

	DBG(2, retval ? "success\n" : "failed\n");

	ti->l.layed_out = retval;	

	return retval;
}

int update_icon_grid_placement(struct TrayIcon *ti)
{
	struct IconPlacement *icn_pmt = NULL;
	
	icn_pmt = find_placement(&ti->l);

	DBG(4, "searching for placement for (%d,%d)\n",
			ti->l.grd_rect.x, ti->l.grd_rect.y);
	
	if (icn_pmt != NULL && icn_pmt->grow.x == 0 && icn_pmt->grow.y == 0) {
		DBG(4, "placement: (%d,%d)\n", icn_pmt->pos.x, icn_pmt->pos.y);
		if (!update_grid_pos(ti, &icn_pmt->pos) &&
			settings.minimal_movement) 
		{
			return 1;
		}
	}

	return 0;
}

int remove_icon_from_grid(struct TrayIcon *ti)
{
	forall_icons_from(ti->next, &update_icon_grid_placement);	
	sort_icons(&ti_cmp);
	forall_icons(&grid2window);
	return 1;
}

int unset_layed_out_flag(struct TrayIcon *ti)
{
	ti->l.layed_out = 0;
	return 0;
}

int add_icon_to_grid_wrapper(struct TrayIcon *ti)
{
	int rc = add_icon_to_grid(ti);
	return !rc;
}

int recreate_grid()
{
	grid_sz.x = 0;
	grid_sz.y = 0;

	forall_icons(&unset_layed_out_flag);
	forall_icons(&add_icon_to_grid_wrapper);
	return 1;
}

int update_grid(struct TrayIcon *ti)
{
	struct IconPlacement *ip = NULL;

	forall_icons_from(ti, &unset_layed_out_flag);
	
/*    WND2GRID(ti->l);*/

/*    new_icon_placement(&ip, ti->l.grd_rect.x, ti->l.grd_rect.y, &ti->l);*/

/*    if (ip != NULL) {*/
/*        if (ip->grow.x || ip->grow.y) {*/
/*            if (!tray_grow(ip->grow)) {*/
/*                DBG(0, "icon resize violates tray size policy");*/
/*            }*/
/*        }*/
/*        */
/*        grid_sz.x += ip->ggrow.x;*/
/*        grid_sz.y += ip->ggrow.y;*/
/*        */
/*        free(ip);*/
/*    } else {*/
/*        DBG(0, "Internal error\n");*/
/*    }*/
	
	forall_icons_from(ti, &add_icon_to_grid_wrapper);
	forall_icons(&grid2window);
	sort_icons(&ti_cmp);

	return 1;
}
