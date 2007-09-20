/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* icons.h
* Tue, 24 Aug 2004 12:19:27 +0700
* -------------------------------
* Icon layout implementation
* -------------------------------*/

#ifndef _LAYOUT_H_
#define _LAYOUT_H_

typedef struct ForceEntry_ {
	struct ForceEntry_ *next, *prev;
	int width, height;
	char *pattern;
} ForceEntry;

#define GRAV_E	(1L << 0)
#define GRAV_W	(1L << 1)
#define GRAV_S	(1L << 2)
#define GRAV_N	(1L << 3)

#define GRAV_H	(GRAV_W | GRAV_E)
#define GRAV_V	(GRAV_S | GRAV_N)

#define LAYOUT_V	(1L << 4)
#define LAYOUT_H	(1L << 4)

typedef struct _Layout {
	int gx, gy;   /* grid coords */
	int gw, gh;   /* grid sizes */
	int x, y;      /* real coords
				      inside p    */

	Window p;      /* new parent  */

	int layed_out; /* just a flag */
} Layout;

/* layouts new/existing icon */
int layout_icon(Display *dpy, Window tray, Window w);
/* window resized */
int layout_update(Display *dpy, Window tray);
/* packs layout (relayouts icons to take minimal space) */
int layout_pack(Display *dpy, Window tray);
/* checks if can grow */
int valid_placement(Display *dpy, Window tray, int g_request_x, int grow_request_y);
/* brute-force window resize */
int force_wnd_size(Display *dpy, Window wnd, int w, int h);

#endif
