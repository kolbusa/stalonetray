/* vim:tabstop=4
 */

/*
 * Manipulations with reparented windows --- tray icons
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "icons.h"
#include "debug.h"
#include "xembed.h"

#define update_val(dest,val,flag)	{flag = flag || ((val) != (dest)); dest = (val);};

/* these are configurable via cli */
int gravity = (GRAV_N | GRAV_W);
int vertical = 0;
int icon_size = 22;
int icon_spacing = 2;
int border_width = 2;
int force = 0;

typedef struct _TrayIcon {
	struct _TrayIcon *next, *prev;
	unsigned char *baloon; /* baloon text for this icon */
	int baloon_id;
	int baloon_len;
	int baloon_rcvd;
	int needs_reparent;
	int needs_move;
	Window w;
	int x, y; /* coords */
} TrayIcon;

TrayIcon *head = NULL;
int tray_full = 0;
int icon_count = 0;

int new_icon_baloon(Window w, int len)
{
}

int update_icon_ballon(Window w, char *data)
{
}

int add_icon(Display *dpy, Window tray, Window w)
{
	TrayIcon *new_icon, *tmp;
	XSizeHints xsh;

	debugmsg(DBG_CLS_ICONS, 0, "add_icon(%u)\n", w);

	for (tmp = head; tmp != NULL; tmp = tmp->next)
		if (tmp->w == w) { /* I dunno why, but this sh.t happens */
			debugmsg(DBG_CLS_ICONS, 2, "\t%u already there --- not adding\n", w);
			return -1;
		}
	
	if ((new_icon = (TrayIcon *)malloc(sizeof(TrayIcon))) == NULL) {
		debugmsg(DBG_CLS_ICONS, 0, "Out of memory");
		return -1;
	}

	new_icon->w = w;
	new_icon->baloon = NULL;
	new_icon->baloon_len = 0;
	new_icon->x = -1;
	new_icon->needs_reparent = 1;

	/* brute-force resizing (configurable): set min_width\min_height */
	if (force) {
		xsh.flags = PMinSize;
		xsh.min_width = icon_size;
		xsh.min_height = icon_size;
		XSetWMNormalHints(dpy, w, &xsh);
	}
	
	XResizeWindow(dpy, w, icon_size, icon_size);

	/* since relayout_icons is called more frequently than add_icon, we 
	 * will add new icon to the tail of the list here, rather than traverse
	 * it from the tail in relayout_icons */
	for (tmp = head; tmp != NULL && tmp->next != NULL; tmp = tmp->next);
	new_icon->prev = tmp;
	new_icon->next = NULL;
	if (tmp != NULL)
		tmp->next = new_icon;
	else
		head = new_icon;

	icon_count++;
	return 0;
}

int rem_icon(Display *dpy, Window tray, Window w)
{
	TrayIcon *tmp;
	debugmsg(DBG_CLS_ICONS, 1, "rem_icon(%u)\n", w);
	for (tmp = head; tmp != NULL; tmp = tmp->next)
		if (tmp->w == w) {
			if (tmp->prev != NULL)
				tmp->prev->next = tmp->next;
			if (tmp->next != NULL)
				tmp->next->prev = tmp->prev;
			if (tmp == head)
				head = tmp->next;
			if (tmp->baloon != NULL)
				free(tmp->baloon);
			/*
			XReparentWindow(dpy, tmp->w, RootWindow(dpy, DefaultScreen(dpy)), 
				tmp->x * (icon_size + icon_spacing), tmp->y * (icon_size + icon_spacing));
			*/
			free(tmp);
			debugmsg(DBG_CLS_ICONS, 3, "\tsuccess\n");
			icon_count--;
			return 0;
		}
	return -1;
}

void clean_icons(Display *dpy)
{
	TrayIcon *tmp1 = head, *tmp2 = head;

	if (head == NULL) return;

	debugmsg(DBG_CLS_ICONS, 0, "clean_icons()\n");
	
	while(tmp1) {
		tmp1 = tmp1->next;
		/* TODO: check this out: where is the reparented window? */
		xembed_window_deactivate(dpy, tmp2->w);
		XUnmapWindow(dpy, tmp2->w);
		XReparentWindow(dpy, tmp2->w, RootWindow(dpy, DefaultScreen(dpy)), 
				100 + tmp2->x * (icon_size + icon_spacing), 100 + tmp2->y * (icon_size + icon_spacing));
		XSync(dpy, False);
		free(tmp2);
		tmp2 = tmp1;
	}
	head = NULL;
	icon_count = 0;
}

int relayout_icons(Display *dpy, Window tray, int width, int height, int force)
{
	XSizeHints xsh;
	TrayIcon *tmp;
	int x = 0, y = 0, mx = 0, my = 0;
	unsigned long xembed_flags;

	debugmsg(DBG_CLS_ICONS, 0, "relayout_icons(width=%u, height=%u)\n", width, height);
	debugmsg(DBG_CLS_ICONS, 7, "\tgravity=%u, vertical=%u\n", gravity, vertical);

	/* recalculate icons positions */
	for (tmp = head; tmp != NULL; tmp = tmp->next) {
		tmp->needs_move = 0;
		update_val(tmp->x, x, tmp->needs_move);
		update_val(tmp->y, y, tmp->needs_move);

		my = (y > my) ? y : my;
		mx = (x > mx) ? x : mx;

		/* gravity-dependent coords inc.*/
		debugmsg(DBG_CLS_ICONS, 5, "\t%u@(%i, %i)\n", tmp->w, tmp->x, tmp->y);
		if (!vertical) {
			x++;
			if (border_width + (x + 1) * icon_size + x * icon_spacing > width - border_width) {
				y++;
				x = 0;
			}
		} else {
			y++;
			if (border_width + (y + 1) * icon_size + y * icon_spacing > height - border_width) {
				y = 0;
				x++;
			}
		}
	}

	/* check if the tray is full and needs to grow */
	if (width - (x * (icon_size + icon_spacing) + 2 * border_width) < icon_size ||
		height - (y * (icon_size + icon_spacing) + 2 * border_width) < icon_size)
		tray_full = 1;
	else
		tray_full = 0;
	
	/* move as apropriate if needed */	
	for (tmp = head; tmp != NULL; tmp = tmp->next) {
		if (!tmp->needs_reparent && !tmp->needs_move && !force)
			continue;
		
		/* gravity-aware placement */
		if (gravity & GRAV_V)
			y = height - border_width - (tmp->y + 1) * icon_size - tmp->y * icon_spacing;
		else
			y = border_width + tmp->y * icon_size + tmp->y * icon_spacing;
		
		if (gravity & GRAV_H)
			x = width - border_width - (tmp->x + 1) * icon_size - tmp->x * icon_spacing;
		else
			x = border_width + tmp->x * icon_size + tmp->x * icon_spacing;

		debugmsg(DBG_CLS_ICONS, 5, "\t%u: to be placed at (%i,%i)\n", tmp->w, x, y);

		if (tmp->needs_reparent) {
			debugmsg(DBG_CLS_ICONS, 6, "\t\treparenting\n");
			XReparentWindow(dpy, tmp->w, tray, x, y);
			tmp->needs_reparent = 0;

			xembed_flags = xembed_get_info(dpy, tmp->w, NULL);
			
			if (xembed_flags & XEMBED_MAPPED)
				XMapRaised(dpy, tmp->w);
			
			xembed_embeded_notify(dpy, tray, tmp->w);
			xembed_window_activate(dpy, tmp->w);
			xembed_focus_in(dpy, tmp->w, XEMBED_FOCUS_CURRENT);
		} else {
			debugmsg(DBG_CLS_ICONS, 6, "\t\tmoving\n");
			XMoveWindow(dpy, tmp->w, x, y);
			tmp->needs_move = 0;
			XFlush(dpy);
		};
	};


	/* FIXME: minimal tray size calc... Hm does not work */
	xsh.flags = PMinSize;
	xsh.min_width = mx * (icon_spacing + icon_size) + icon_size + border_width * 2;
	xsh.min_height = my * (icon_spacing + icon_size) + icon_size + border_width * 2;
	
	XSetWMNormalHints(dpy, tray, &xsh);

	return tray_full;
}

