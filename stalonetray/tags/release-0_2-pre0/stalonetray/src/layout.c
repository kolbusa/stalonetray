/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * icons.h
 * Tue, 24 Aug 2004 12:19:48 +0700
 * -------------------------------
 * Icon layout implementation
 * -------------------------------*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "layout.h"

#include "debug.h"
#include "icons.h"
#include "list.h"
#include "settings.h"
#include "embed.h"
#include "string.h"

typedef struct _PlacementQuery {
	struct _PlacementQuery *next, *prev;

	Layout l;
	
	int grow_request_x;
	int grow_request_y;

} PlacementQuery;

extern TrayIcon *icons_head;
extern int grow(int, int);

/* prototypes */
int grid_icon(Display *dpy, Window tray, TrayIcon *ti);
int grid_to_window(Display *dpy, Window tray, PlacementQuery *pq);
void update_grid();
int force_wnd_size(Display *dpy, Window wnd, int w, int h);
char *strcasestr (const char *haystack, const char *needle);

void force_icon_size(Display *dpy, Window w)
{
	ForceEntry *tmp = settings.f_head;
	XClassHint xch;
	int s;

	trap_errors();

	if (!XGetClassHint(dpy, w, &xch))
		return;
	
	if (s = untrap_errors()) {
		DBG(0, "0x%x gone (%d)?\n", w, s);
		return;
	}
	
	DBG(3, "res_name=%s, res_class=%s\n", xch.res_name, xch.res_class);

	for (; tmp != NULL; tmp = tmp->next) {	
		char *cls, *nam;
		/* TODO: complex shell-like patterns */
		if (strcasestr(xch.res_name, tmp->pattern) == xch.res_name ||
			strcasestr(xch.res_class, tmp->pattern) == xch.res_class)
		{
			DBG(0, "forcing %s to %dx%d\n", xch.res_name, tmp->width, tmp->height);
			force_wnd_size(dpy, w, tmp->width, tmp->height);
			break;
		}
	}

	XFree(xch.res_name);
	XFree(xch.res_class);
}

#define 	MAX_WAIT_ROUNDS	10

/* layouts new/existing icon */
int layout_icon(Display *dpy, Window tray, Window w)
{
	TrayIcon *tmp;
	XWindowAttributes xwa;
	int s, counter = 0;

	if ((tmp = find_icon(w)) == NULL) {
		return 0;
	}

	s = 0;

	do {

		trap_errors();
		XGetWindowAttributes(dpy, w, &xwa);
		DBG(4, "current 0x%x geometry: %dx%d\n", w, xwa.width, xwa.height);
		if (s = untrap_errors()) {
			DBG(0, "0x%x gone (%d)?\n", w, s);
			return 0;
		}

		if (xwa.width > 1 && xwa.height > 1) {
			s = 1;
			break;
		}

		usleep(50000L);

		counter ++;
	} while (counter < MAX_WAIT_ROUNDS + 1);

	if (s == 0 && tmp->cmode != CM_KDE) {
		return 0;
	}
	
	if (tmp->cmode == CM_KDE) {
		force_wnd_size(dpy, tmp->w, 24, 24);
	}
	
	force_icon_size(dpy, tmp->w);

	return grid_icon(dpy, tray, tmp);
}

/* window resized */
int layout_update(Display *dpy, Window tray)
{
	TrayIcon *tmp;
	PlacementQuery pq;
	int s;

	for (tmp = icons_head; tmp != NULL; tmp = tmp->next) {
		if (!tmp->l.layed_out) {
			continue;
		}

		pq.l = tmp->l;
		grid_to_window(dpy, tray, &pq);
		if (pq.l.x != tmp->l.x || pq.l.y != tmp->l.y) {
			tmp->l.x = pq.l.x;
			tmp->l.y = pq.l.y;

			trap_errors();
			XMoveWindow(dpy, tmp->l.p, tmp->l.x, tmp->l.y);
			if (s = untrap_errors()) {
				DBG(1, "failed to update 0x%x position (%d)\n", tmp->w, s);
			}
		}
	}
	
}

/**********************************************************************************/

/* util */
int force_wnd_size(Display *dpy, Window wnd, int w, int h)
{
	XSizeHints xsh;
	XWindowAttributes xwa;
	int width, height;
	int s;
	long supplied;
	
#ifdef DEBUG
	{
		XWindowAttributes xwa;

		trap_errors();
		XGetWindowAttributes(dpy, wnd, &xwa);
		DBG(3, "current 0x%x geometry: %dx%d\n", wnd, xwa.width, xwa.height);
		untrap_errors();
	}
#endif

	trap_errors();

	xwa.width = w + 1;
	xwa.height = h + 1;

	XGetWindowAttributes(dpy, wnd, &xwa);

	if (xwa.width <= w && xwa.height <= h) {
		return 1;
	}

	width = w != 0 ? w : xwa.width;
	height = h != 0 ? h : xwa.height;

	DBG(3, "forcing to %dx%d\n", width, height);
	
	xsh.flags = PMinSize | PBaseSize | PAspect;
	xsh.min_width = width;
	xsh.min_height = height;
	xsh.base_width = width;
	xsh.base_height = xwa.height;
	xsh.min_aspect.x = 0;
	xsh.min_aspect.y = 1;
	xsh.max_aspect.x = 1000;
	xsh.max_aspect.y = 1;
	XSetWMNormalHints(dpy, wnd, &xsh);
	
	usleep(50000L);
	
	XResizeWindow(dpy, wnd, width, height);
	
	if (s = untrap_errors()) {
		DBG(0, "failed(%d)\n", s);
		return 0;
	}

#ifdef DEBUG
	{
		XWindowAttributes xwa;

		usleep(50000L);

		trap_errors();
		XGetWindowAttributes(dpy, wnd, &xwa);
		DBG(3, "new 0x%x geometry: %dx%d\n", wnd, xwa.width, xwa.height);
		untrap_errors();
	}
#endif
	return 1;
}


/******************************************************
 * Here comes the Grid                                *
 * ************************************************** *
 * XXX: I'm SLOOW. I need hard optimizations, PLEASE! *
 ******************************************************/

static int g_width = 0;
static int g_height = 0;

extern int grow_gravity;
extern int max_tray_width;
extern int max_tray_height;

extern void set_constraints(int, int);

/* packs layout (relayouts icons to take minimal space) */
int layout_pack(Display *dpy, Window tray)
{
	TrayIcon *tmp;
	int s;

	for (tmp = icons_head; tmp != NULL; tmp = tmp->next) {
		tmp->l.layed_out = 0;
	}

	g_width = 0; g_height = 0;

	for (tmp = icons_head; tmp != NULL; tmp = tmp->next) {
		grid_icon(dpy, tray, tmp);
		trap_errors();
		XMoveWindow(dpy, tmp->l.p, tmp->l.x, tmp->l.y);
		if (s = untrap_errors()) {
				DBG(0, "0x%x gone? (%d)\n", tmp->w, s);
		}
	}
}

void grid_update()
{
	TrayIcon *tmp;
	int w = 0, h = 0;
	
	for (tmp = icons_head; tmp != NULL; tmp = tmp->next) {
		w = (w < tmp->l.gx + tmp->l.gw) ? tmp->l.gx + tmp->l.gw : w;
		h = (h < tmp->l.gy + tmp->l.gh) ? tmp->l.gy + tmp->l.gh : h;
	}
}

PlacementQuery * pq_head = NULL;

int add_placement(int x, int y, Display *dpy, Window tray, TrayIcon *ti)
{
	PlacementQuery *pq;
	
	if ((pq = (PlacementQuery *) malloc(sizeof(PlacementQuery))) == NULL) {
		return 0;
	}

	pq->l = ti->l;
	
	pq->l.gx = x;
	pq->l.gy = y;

	grid_to_window(dpy, tray, pq);

	LIST_ADD_ITEM(pq_head, pq);
	
	return 1;	
}

int valid_placement(Display *dpy, Window tray, int grow_request_x, int grow_request_y)
{
	XWindowAttributes xwa;

	XGetWindowAttributes(dpy, tray, &xwa);

	if (settings.grow_gravity == 0) {
		return 0;
	}
	
	if ((settings.max_tray_width > 0 && xwa.width + grow_request_x > settings.max_tray_width) ||
		(settings.max_tray_height > 0 && xwa.height + grow_request_y > settings.max_tray_height) ||
		(grow_request_x > 0 && (!(settings.grow_gravity & GRAV_H))) ||
		(grow_request_y > 0 && (!(settings.grow_gravity & GRAV_V))))
	{
		return 0;
	} 

	return 1;	
}

int cmp_placements(PlacementQuery *pq0, PlacementQuery *pq1)
{
	int s0 = pq0->grow_request_x + pq0->grow_request_y;
	int s1 = pq1->grow_request_x + pq1->grow_request_y;

	/* pq0 < pq1 ==> return -1;
	 * pq0 ~ pq1 ==> return 0;
	 * pq0 > pq1 ==> return 1; */

	return  (s0 == s1) ? 0 : ((s0 < s1) ? -1 : 1);
}

void check_placements(Display *dpy, Window tray, TrayIcon *ti)
{
	PlacementQuery *tmp_hd = NULL, *tmp, *curr;

	ti->l.layed_out = 0;

	/* XXX: use better sorting algorythm */
	curr = pq_head;
	while (curr != NULL) {
		tmp = NULL;
		
		if (!valid_placement(dpy, tray, curr->grow_request_x, curr->grow_request_y)) {
			tmp = curr;
		}
		curr = curr->next;

		if (tmp != NULL) {
			LIST_DEL_ITEM(pq_head, tmp);
			free(tmp);
		}
	}
	
	if (pq_head == NULL) {
		return;
	}
	
	while (pq_head != NULL) {
		tmp = pq_head;
		
		for (curr = pq_head->next; curr != NULL; curr = curr->next) {
			if (cmp_placements(tmp, curr) <= 0) {
				tmp = curr;
			}
		}

		LIST_DEL_ITEM(pq_head, tmp);
		LIST_ADD_ITEM(tmp_hd, tmp);
	}
	
	if (tmp_hd->grow_request_x > 0 || tmp_hd->grow_request_y > 0) {
		grow(tmp_hd->grow_request_x, tmp_hd->grow_request_y);
		usleep(100000L);
		XSync(dpy, False); /* to update trays` size */
		layout_update(dpy, tray);
		grid_to_window(dpy, tray, tmp_hd);
		grid_update();
	}

	ti->l.x = tmp_hd->l.x;
	ti->l.y = tmp_hd->l.y;
	ti->l.gx = tmp_hd->l.gx;
	ti->l.gy = tmp_hd->l.gy;

	ti->l.layed_out = 1;

	while (tmp_hd != NULL) {
		tmp = tmp_hd;
		LIST_DEL_ITEM(tmp_hd, tmp);
		free(tmp);
	}
}

/* checks if the cell (x,y) is unoccupied */
int grid_cell_free(int x, int y)
{
	TrayIcon *tmp;
	
	for (tmp = icons_head; tmp != NULL; tmp = tmp->next)
		if (tmp->l.layed_out && tmp->l.gx <= x && x < tmp->l.gx + tmp->l.gw
		                     && tmp->l.gy <= y && y < tmp->l.gy + tmp->l.gh) {
			return 0;
		}
	
	return 1;
}

/* checks if the w*h rect fits starting from the (x,y) cell */
int grid_check_cell(int x, int y, int w, int h)
{
	int i, j; /* what a stamp! */

	for (i = x; i < x + w; i++)
		for (j = y; j < y + h; j++)
			if (!grid_cell_free(i, j))
				return 0;
	return 1;
}

/* converts grid coords to window */
int grid_to_window(Display *dpy, Window tray, PlacementQuery *pq)
{
	XWindowAttributes xwa;
	int s;

	DBG(2, "grid coords: (%d, %d)\n", pq->l.gx, pq->l.gy);

	trap_errors();
	
	XGetWindowAttributes(dpy, tray, &xwa);
	
	if (s = untrap_errors()) {
		DBG(0, "window gone? (%d)\n", s);
		return 0;
	}

	DBG(3, "tray window dimensions: (%d, %d)\n", xwa.width, xwa.height);

	pq->grow_request_x = 0;
	pq->grow_request_y = 0;

	/* ok. no spacing at this time. and ever */
	
	if (settings.gravity & GRAV_E) {
		pq->l.x = xwa.width - (pq->l.gx + pq->l.gw) * settings.icon_size;
	} else {
		pq->l.x = pq->l.gx * settings.icon_size;
	}

	if (settings.gravity & GRAV_S) {
		pq->l.y = xwa.height - (pq->l.gy + pq->l.gh) * settings.icon_size;
	} else {
		pq->l.y = pq->l.gy * settings.icon_size;
	}

	if (pq->l.x < 0 || pq->l.y < 0 ||
		pq->l.x + pq->l.gw * settings.icon_size > xwa.width || pq->l.y + pq->l.gh * settings.icon_size > xwa.height)
	{	/* XXX: this code needs review and testing */
		if (pq->l.x < 0) {
			pq->grow_request_x = -1 * pq->l.x;
		}
		if (pq->l.x + pq->l.gw * settings.icon_size > xwa.width) {
			pq->grow_request_x = pq->l.x + pq->l.gw * settings.icon_size - xwa.width;
		}
		if (pq->l.y < 0) {
			pq->grow_request_y = -1 * pq->l.y;
		}
		if (pq->l.y + pq->l.gh * settings.icon_size > xwa.height) {
			pq->grow_request_y = pq->l.y + pq->l.gh * settings.icon_size - xwa.height;
		}
		DBG(3, "grow_request(%d, %d)\n", pq->grow_request_x, pq->grow_request_y);
		return 0;
	}

	DBG(2, "window coords are (%d, %d)\n", pq->l.x, pq->l.y);

	return 1;
};

int sort_icons(Display *dpy)
{
	TrayIcon *tmp = icons_head, *min;
	int x, y, x_min, y_min;


	while (1) {
		min = icons_head;
		x_min = settings.vertical ? min->l.gy : min->l.gx;
		y_min = settings.vertical ? min->l.gx : min->l.gy;

#ifdef DEBUG
		for (tmp = icons_head; tmp != NULL; tmp = tmp->next) {
			XClassHint xch;

			trap_errors();

			XGetClassHint(dpy, tmp->w, &xch);
		
			untrap_errors();
		
			DBG(3, "%s (0x%x): (%d, %d)\n", xch.res_name, tmp->w, tmp->l.gx, tmp->l.gy);

			XFree(xch.res_name);
			XFree(xch.res_class);
		}
#endif

		for (tmp = icons_head; tmp != NULL; tmp = tmp->next) {
			x = settings.vertical ? tmp->l.gy : tmp->l.gx;
			y = settings.vertical ? tmp->l.gx : tmp->l.gy;

			if (y < y_min || (y == y_min && x < x_min)) {
				DBG(3, "(%d, %d) < (%d, %d)\n", x, y, x_min, y_min);
				min = tmp;
			} else {
				DBG(3, "(%d, %d) >= (%d, %d)\n", x, y, x_min, y_min);
			}
		}

		if (min != icons_head) {
			LIST_DEL_ITEM(icons_head, min);
			LIST_ADD_ITEM(icons_head, min);
		} else {
			break;
		}
	}
#ifdef DEBUG
	for (tmp = icons_head; tmp != NULL; tmp = tmp->next) {
		XClassHint xch;

		trap_errors();

		XGetClassHint(dpy, tmp->w, &xch);
		
		untrap_errors();
		
		DBG(3, "%s (0x%x): (%d, %d)\n", xch.res_name, tmp->w, tmp->l.gx, tmp->l.gy);

		XFree(xch.res_name);
		XFree(xch.res_class);
	}
#endif
}

/* finds a place for a icon */
int grid_icon(Display *dpy, Window tray, TrayIcon *ti)
{
	XWindowAttributes xwa;
	int x, y, s;
	int g_width_old = g_width;
	int g_height_old = g_height;

	DBG(0, "0x%x, %d, %s\n", ti->w, settings.icon_size, settings.vertical ? "vertical" : "horizontal");
	
	if (!ti->l.layed_out) { /* needs to calculate w/h */
		trap_errors();
		
		XGetWindowAttributes(dpy, ti->w, &xwa);

		if (s = untrap_errors()) {
			DBG(0, "0x%x gone (%d)?\n", ti->w, s);
			return 0;
		}

		DBG(2, "0x%x's geometry %dx%d\n", ti->w, xwa.width, xwa.height);
		
		ti->l.gw = xwa.width / settings.icon_size + (xwa.width % settings.icon_size != 0);
		ti->l.gh = xwa.height / settings.icon_size + (xwa.height % settings.icon_size != 0);

		DBG(3, "gw=%d, gh=%d\n", ti->l.gw, ti->l.gh);
	}
	
	if (g_width == 0) { /* we have no grid (==> g_height == 0) */
		DBG(2, "creating new grid\n");
		add_placement(0, 0, dpy, tray, ti);
		g_width = ti->l.gw;
		g_height = ti->l.gh;
	} else {
		DBG(3, "searching for a place in a grid\n");

		/* common fallbacks */
		if (settings.vertical) {
			add_placement(g_width, 0, dpy, tray, ti);
			add_placement(0, g_height, dpy, tray, ti);
		} else {
			add_placement(0, g_height, dpy, tray, ti);
			add_placement(g_width, 0, dpy, tray, ti);
		}
		
		x = g_width - 1;
		y = g_height - 1;
		do {
			if (grid_check_cell(x, y, ti->l.gw, ti->l.gh)) {
				DBG(10, "found (%d, %d)\n", x, y);
				add_placement(x, y, dpy, tray, ti);
			}
			
			x = settings.vertical ? (y == 0 ? x - 1 : x) : (x == 0 ? x = g_width - 1 : x - 1);
			y = settings.vertical ? (y == 0 ? g_height - 1 : y - 1 ) : ( x == g_width - 1 ? y - 1 : y);
			
			if (x < 0 || y < 0)
				break;
		} while (1);

	}

	check_placements(dpy, tray, ti);
	
	if (!ti->l.layed_out) {
		DBG(1, "failed to layout 0x%x\n", ti->w);
		return 0;
	}

	g_width = (g_width >= ti->l.gx + ti->l.gw ) ? g_width : ti->l.gx + ti->l.gw;
	g_height = (g_height >= ti->l.gy + ti->l.gh ) ? g_height : ti->l.gy + ti->l.gh;

	set_constraints((g_width + 1) * settings.icon_size, (g_height + 1) * settings.icon_size);
	
	DBG(0, "layed 0x%x to (%d,%d)\n", ti->w, ti->l.gx, ti->l.gy);
	DBG(3, "g_width=%d, g_height=%d\n", g_width, g_height);

	sort_icons(dpy);

	return 1;
}

