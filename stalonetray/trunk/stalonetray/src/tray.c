/* ************************************
 * vim:tabstop=4:shiftwidth=4:cindent:fen:fdm=syntax
 * tray.c
 * Tue, 07 Mar 2006 10:36:10 +0600
 * ************************************
 * tray functions
 * ************************************/

#include "config.h"

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#ifdef XPM_SUPPORTED
#include <X11/xpm.h>
#endif

#include <unistd.h>

#include "common.h"
#include "tray.h"
#include "settings.h"
#include "xutils.h"
#include "wmh.h"
#include "embed.h"
#include "image.h"
#include "icons.h"

#ifndef NO_NATIVE_KDE
#include "kde_tray.h"
#endif

#include "debug.h"

void tray_init()
{
	tray_data.tray = None;
	tray_data.dpy = NULL;
	tray_data.async_dpy = NULL;
	tray_data.bg_pmap = None;
	tray_data.xa_xrootpmap_id = None;
	tray_data.xa_xsetroot_id = None;
	tray_data.xsh.flags = (PSize | PPosition | PWinGravity);
	tray_data.xsh.x = 100;
	tray_data.xsh.y = 100;
	tray_data.xsh.width = 10;
	tray_data.xsh.height = 10;
	tray_data.kde_tray_old_mode = 0;
}

#ifdef XPM_SUPPORTED
int tray_init_pixmap_bg()
{
	XpmAttributes xpma;
	Pixmap mask = None;
	int rc;
	xpma.valuemask = XpmReturnAllocPixels;
	rc = XpmReadFileToPixmap(
			tray_data.dpy, 
			tray_data.tray, 
			settings.bg_pmap_path, 
			&tray_data.bg_pmap, 
			&mask, 
			&xpma);
	if (rc != XpmSuccess) {
		DIE(("Could not read background pixmap.\n")); 
		return FAILURE;
	}
	/* Ignore the mask */
	if (mask != None) XFreePixmap(tray_data.dpy, mask);
	tray_data.bg_pmap_height = xpma.height;
	tray_data.bg_pmap_width = xpma.width;
	DBG(8, ("created background pixmap\n"));
	return SUCCESS;
}
#endif

/* Mostly from FVWM:fvwm/colorset.c:172 */
Pixmap tray_get_root_pixmap(Atom prop)
{
	Atom type;
	int format;
	unsigned long length, after;
	unsigned char *reteval = NULL;
	int ret;
	Pixmap pix = None;
	Window root_window = XRootWindow(tray_data.dpy, DefaultScreen(tray_data.dpy));
	ret = XGetWindowProperty(tray_data.dpy, 
			root_window, 
			prop, 
			0L, 
			1L, 
			False, 
			XA_PIXMAP,
			&type, 
			&format, 
			&length, 
			&after, 
			&reteval);
	if (x11_ok() && ret == Success && type == XA_PIXMAP && format == 32 && length == 1 && after == 0) 
		pix = (Pixmap)(*(long *)reteval);
	if (reteval) XFree(reteval);
	return pix;
}

/* Originally from FVWM:fvwm/colorset.c:195 */
int tray_update_root_bg_pmap(Pixmap *pmap, int *width, int *height)
{
	unsigned int w = 0, h = 0;
	int rc = 0;
	XID dummy;
	Pixmap pix = None;
	
	/* Retrive root image pixmap */
	/* Try _XROOTPMAP_ID first */
	if (tray_data.xa_xrootpmap_id != None)
		pix = tray_get_root_pixmap(tray_data.xa_xrootpmap_id);
	/* Else try _XSETROOT_ID */
	if (!pix && tray_data.xa_xsetroot_id != None)
		pix = tray_get_root_pixmap(tray_data.xa_xsetroot_id);
	/* Get root pixmap size */
	if (pix) 
		rc = XGetGeometry(tray_data.dpy, 
				pix, 
				&dummy, 
				(int *)&dummy, 
				(int *)&dummy,
				&w, 
				&h, 
				(unsigned int *)&dummy, 
				(unsigned int *)&dummy);

	if (!x11_ok() || !pix || !rc) {
		DBG(0, ("could not update root background pixmap\n"));
		return FAILURE;
	}
	
	*pmap = pix;
	if (width != NULL) *width = w;
	if (height != NULL) *height = h;
	
	DBG(6, ("got new root pixmap: 0x%lx (%ix%i)\n", pix, w, h));
	return SUCCESS;
}

/* XXX: most of Pixmaps are not really needed. Use XImages instead.
 * GCs, XImages and Pixmaps stay allocated during cleanup, valgrind
 * complains. Who cares?
 * TODO: move pixmaps/GCs into tray_data and free them during the exit. */
int tray_update_bg(int update_pixmap)
{
	static Pixmap root_pmap = None;
	static Pixmap bg_pmap = None; 
	static Pixmap final_pmap = None;
	static GC root_gc = None;
	static GC bg_gc = None;
	static GC final_gc = None;
	static int old_width = -1, old_height = -1;

	struct Rect clr;		/* clipping rectangle */
	struct Rect clr_loc;	/* clipping rectangle in local coords */

	XImage *bg_img;

	int bg_pmap_updated = False;

#define make_gc(pmap,gc) do { XGCValues gcv; \
		if (gc != None) XFreeGC(tray_data.dpy, gc); \
		gcv.graphics_exposures = False; \
		gc = XCreateGC(tray_data.dpy, pmap, GCGraphicsExposures, &gcv); \
	} while(0)

#define make_pmap(pmap) do { \
		if (pmap != None) XFreePixmap(tray_data.dpy, pmap); \
		pmap = XCreatePixmap(tray_data.dpy, tray_data.tray, \
				tray_data.xsh.width, tray_data.xsh.height, \
				DefaultDepth(tray_data.dpy, DefaultScreen(tray_data.dpy))); \
	} while(0)

#define recreate_pixmap(pmap,gc) do { \
		make_pmap(pmap); \
		make_gc(pmap, gc); \
	} while(0)

	if (!settings.transparent && !settings.pixmap_bg) return SUCCESS;

	/* Calculate clipping rect */
	clr.x = cutoff(tray_data.xsh.x, 0, tray_data.root_wnd.width);
	clr.y = cutoff(tray_data.xsh.y, 0, tray_data.root_wnd.height);
	clr.w = cutoff(tray_data.xsh.x + tray_data.xsh.width, 0, tray_data.root_wnd.width) - clr.x;
	clr.h = cutoff(tray_data.xsh.y + tray_data.xsh.height, 0, tray_data.root_wnd.height) - clr.y;

	/* There's no need to update background if tray is not visible */
	/* TODO: visibility is better to be tracked using some events */
	if (clr.w == 0 || clr.h == 0) return SUCCESS;

	/* Calculate local clipping rect */
	clr_loc.x = clr.x - tray_data.xsh.x;
	clr_loc.y = clr.y - tray_data.xsh.y;
	clr_loc.w = clr.w; 
	clr_loc.h = clr.h;

	if (old_width != tray_data.xsh.width || old_height != tray_data.xsh.height || final_pmap == None) 
		recreate_pixmap(final_pmap, final_gc);

	/* Update root pixmap if asked and necessary */
	if ((root_pmap == None || update_pixmap) &&
			(settings.transparent || settings.fuzzy_edges)) 
	{
		if (!tray_update_root_bg_pmap(&root_pmap, NULL, NULL)) {
#if 0
			/* If we could not get root pmap, stop using it */
			settings.transparent = False;
			settings.fuzzy_edges = False;
			ERR(("Could not get root pimap\n"
						"Use Esetroot or any compatible program to set wallpaper\n"
						"Root transparency/fuzzy edges disabled\n"));
#else
			/* More gracefull solution */
			DBG(9, ("still waiting for root pixmap\n"));
#endif
			return SUCCESS;
		} else
			make_gc(root_pmap, root_gc);
	}

	/* We don't have to update background if only window position has
	 * changed unless we depend on root pixmap */
	if (tray_data.xsh.width == old_width && tray_data.xsh.width == old_height &&
			!settings.transparent && !settings.fuzzy_edges) 
	{
		return SUCCESS;
	}
	
	/* If pixmap bg is used, bg_pmap holds tinted and tiled background pixmap,
	 * so there's no need to update it unless window size has changed */
	if (settings.pixmap_bg && (bg_pmap == None || 
			old_width != tray_data.xsh.width || old_height != tray_data.xsh.height))
	{
		int i, j;
		recreate_pixmap(bg_pmap, bg_gc);
		for (i = 0; i < tray_data.xsh.width / tray_data.bg_pmap_width + 1; i++)
			for (j = 0; j < tray_data.xsh.height / tray_data.bg_pmap_height + 1; j++) {
				XCopyArea(tray_data.dpy, tray_data.bg_pmap, bg_pmap, bg_gc, 
						0, 0, tray_data.bg_pmap_width, tray_data.bg_pmap_height, 
						i * tray_data.bg_pmap_width, j * tray_data.bg_pmap_height);
			}
		bg_pmap_updated = True;
	} else if (settings.transparent) {
		recreate_pixmap(bg_pmap, bg_gc);
		XCopyArea(tray_data.dpy, root_pmap, bg_pmap, bg_gc, 
				tray_data.xsh.x, tray_data.xsh.y, 
				tray_data.xsh.width, tray_data.xsh.height, 
				0, 0);
	}

	bg_img = XGetImage(tray_data.dpy, bg_pmap,
			0, 0, tray_data.xsh.width, tray_data.xsh.height,
			XAllPlanes(), ZPixmap);

	/* Tint the image if necessary. If bg_pmap was not updated, tinting
	 * is not needed, since it has been already done */
	if (settings.tint_level && ((settings.pixmap_bg && bg_pmap_updated) || settings.transparent)) {
		image_tint(bg_img, &settings.tint_color, settings.tint_level);
		XPutImage(tray_data.dpy, bg_pmap, bg_gc, bg_img,
				0, 0, 
				0, 0, tray_data.xsh.width, tray_data.xsh.height);
		bg_pmap_updated = False;
	}

	/* Apply fuzzy edges */
	/* XXX: THIS IS UGLY */
	if (settings.fuzzy_edges) {
		static CARD8 *alpha_mask = NULL;
		
		/* Portion of root pixmap under the tray */
		XImage *root_img = XGetImage(tray_data.dpy, root_pmap, 
				clr.x, clr.y, clr.w, clr.h, 
				XAllPlanes(), ZPixmap);

		/* Proxy structure to work on */
		static XImage *tmp_img = NULL;
		static Pixmap tmp_pmap = None;
		static GC tmp_gc = None;	

		/* Alpha mask needs to be updated only on size changes */
		if (old_width != tray_data.xsh.width || old_height != tray_data.xsh.height) {
			recreate_pixmap(tmp_pmap, tmp_gc);
			if (alpha_mask != NULL) free(alpha_mask);
			alpha_mask = image_create_alpha_mask(settings.fuzzy_edges, tray_data.xsh.width, tray_data.xsh.height);
		}

		XPutImage(tray_data.dpy, tmp_pmap, tmp_gc, root_img,
				clr_loc.x, clr_loc.y,
				0, 0, clr_loc.w, clr_loc.h);

		tmp_img = XGetImage(tray_data.dpy, tmp_pmap,
				0, 0, tray_data.xsh.width, tray_data.xsh.height,
				XAllPlanes(), ZPixmap);

		image_compose(bg_img, tmp_img, alpha_mask);
		
		XDestroyImage(root_img);
		XDestroyImage(tmp_img);
	}

	XPutImage(tray_data.dpy, final_pmap, final_gc, bg_img,
			0, 0,
			0, 0, tray_data.xsh.width, tray_data.xsh.height);

	XSetWindowBackgroundPixmap(tray_data.dpy, tray_data.tray, final_pmap);

	old_width = tray_data.xsh.width;
	old_height = tray_data.xsh.height;

	DBG(4, ("Done\n"));

	return x11_ok();
}

void tray_refresh_window(int exposures)
{
	
	DBG(8, ("refreshing tray window\n"));
	icon_list_forall(&embedder_refresh);
	x11_refresh_window(tray_data.dpy, tray_data.tray, tray_data.xsh.width, tray_data.xsh.height, exposures);
}

int tray_update_window_size()
{
	/* XXX: do we need some caching here? */
	XSizeHints xsh;
	unsigned int layout_width, layout_height;
	unsigned int new_width, new_height;

	/* XXX: should'n we bail out if we are not top-level window? 
	 * Or, perhaps, FvwmButtons are going to handle resises some day? */

	layout_get_size(&layout_width, &layout_height);

	if (settings.shrink_back_mode) {
		new_width = layout_width > settings.orig_width ?
							layout_width : settings.orig_width;
		new_height = layout_height > settings.orig_height ?
							layout_height : settings.orig_height;
		xsh.max_width = new_width;
		xsh.max_height = new_height;
	} else {
		new_width = layout_width > tray_data.xsh.width ? 
							layout_width : tray_data.xsh.width;
		new_height = layout_height > tray_data.xsh.height ? 
							layout_height : tray_data.xsh.height;
		xsh.max_width = settings.max_tray_width;
		xsh.max_height = settings.max_tray_height;
	}
	
	DBG(3, ("proposed new window size: %dx%d (old: %dx%d)\n", new_width, new_height, tray_data.xsh.width, tray_data.xsh.height));
	
	xsh.min_width = new_width;
	xsh.min_height = new_height;

	xsh.width_inc = settings.icon_size;
	xsh.height_inc = settings.icon_size;
	xsh.base_width = settings.icon_size;
	xsh.base_height = settings.icon_size;
/*    xsh.win_gravity = settings.win_gravity;*/
	xsh.win_gravity = NorthWestGravity;
	xsh.flags = PResizeInc|PBaseSize|PMinSize|PMaxSize|PWinGravity; 
	XSetWMNormalHints(tray_data.dpy, tray_data.tray, &xsh);

#ifdef DEBUG
	{
		/* TODO: test w/o DEBUG */
		XSizeHints xsh_;
		long flags;
		XGetWMNormalHints(tray_data.dpy, tray_data.tray, &xsh_, &flags);
		DBG(8, ("WMNormalHints: flags=0x%x, gravity=%d, max size=%dx%d\n", 
					flags, 
					xsh_.win_gravity,
					xsh_.max_width,
					xsh_.max_height
					));
	}
#endif


	/* This check helps to avod extra (erroneous) moves of the window when
	 * geometry changes are not updated yet, but tray_update_window_size() was
	 * called once again */
	if (tray_data.xsh.width != new_width || tray_data.xsh.height != new_height) {
		/* Apparently, not every WM (hello, WindowMaker!) handles gravity the
		 * way I have expected (i.e. using it to calculate reference point as
		 * described in ICCM/WM specs). Perhaps, I was dreaming.  So, prior to
		 * resizing trays window, it is necessary to recalculate window
		 * absolute position and shift it according to grow gravity settings */
		x11_get_window_abs_coords(tray_data.dpy, tray_data.tray, &tray_data.xsh.x, &tray_data.xsh.y);
		DBG(8, ("old geometry: %dx%d+%d+%d\n", tray_data.xsh.x, tray_data.xsh.y, tray_data.xsh.width, tray_data.xsh.height));

		if (settings.grow_gravity & GRAV_E) tray_data.xsh.x -= new_width - tray_data.xsh.width;
		if (settings.grow_gravity & GRAV_S) tray_data.xsh.y -= new_height - tray_data.xsh.height;
		tray_data.xsh.width = new_width;
		tray_data.xsh.height = new_height;

		DBG(8, ("new geometry: %dx%d+%d+%d\n", tray_data.xsh.x, tray_data.xsh.y, new_width, new_height));
		
		XResizeWindow(tray_data.dpy, tray_data.tray, new_width, new_height);
		XMoveWindow(tray_data.dpy, tray_data.tray, tray_data.xsh.x, tray_data.xsh.y);
		if (!x11_ok()) {
			DBG(0, ("could not update tray window size\n"));
			return FAILURE;
		}
	} else {
		XResizeWindow(tray_data.dpy, tray_data.tray, 
						tray_data.xsh.width, tray_data.xsh.height);
	}

	return SUCCESS;
}

void tray_create_window(int argc, char **argv)
{
	char *wnd_name = PROGNAME;
	XTextProperty wm_name;

	XSetWindowAttributes xswa;
	
	XClassHint	*xch = NULL;
	XWMHints	*xwmh = NULL;
	XSizeHints	*xsh = NULL;
	
	Atom		net_system_tray_orientation;
	Atom 		orient;

	Atom		protocols_atoms[2];

	tray_data.xa_wm_delete_window = 
		XInternAtom(tray_data.dpy, "WM_DELETE_WINDOW", True);
	tray_data.xa_wm_take_focus = 
		XInternAtom(tray_data.dpy, "WM_TAKE_FOCUS", True);
	tray_data.xa_wm_protocols = 
		XInternAtom(tray_data.dpy, "WM_PROTOCOLS", True);
	tray_data.xa_kde_net_system_tray_windows = 
		XInternAtom(tray_data.dpy, "_KDE_NET_SYSTEM_TRAY_WINDOWS", True);
	tray_data.xa_net_client_list =
		XInternAtom(tray_data.dpy, "_NET_CLIENT_LIST", True);

	tray_data.tray = XCreateSimpleWindow(
						tray_data.dpy, 
						DefaultRootWindow(tray_data.dpy),
						tray_data.xsh.x, tray_data.xsh.y, 
						tray_data.xsh.width, tray_data.xsh.height, 
						0, 
						settings.bg_color.pixel, 
						settings.bg_color.pixel);

	DBG(3, ("created tray`s window: 0x%x\n", tray_data.tray));

	xswa.bit_gravity = settings.bit_gravity;
	xswa.win_gravity = settings.win_gravity;
	xswa.backing_store = settings.parent_bg ? NotUseful : WhenMapped;
	tray_data.xsh.win_gravity = settings.win_gravity;

	XChangeWindowAttributes(tray_data.dpy, 
			tray_data.tray, 
			CWBitGravity | CWWinGravity | CWBackingStore, 
			&xswa);

	if (settings.transparent || settings.parent_bg || settings.fuzzy_edges) {
		tray_data.xa_xrootpmap_id = XInternAtom(tray_data.dpy, "_XROOTPMAP_ID", False);
		tray_data.xa_xsetroot_id = XInternAtom(tray_data.dpy, "_XSETROOT_ID", False);
	}
		
	if (settings.parent_bg)
		XSetWindowBackgroundPixmap(tray_data.dpy, tray_data.tray, ParentRelative);

	if (settings.transparent) {
		if (tray_data.xa_xrootpmap_id == None && tray_data.xa_xsetroot_id == None) {
			DBG(0, ("transparent background could not be supported.\n"));
			DBG(0, ("use compatible tool to set wallpaper --- e.g. Esetroot, fvwm-root, etc.\n"));
		} else 
			tray_update_bg(True);
	}

	if (XmbTextListToTextProperty(tray_data.dpy, &wnd_name, 1, XTextStyle, &wm_name) != Success)
		DIE(("Invalid window name (THIS IS A BUG)\n"));

	if ((xwmh = XAllocWMHints()) == NULL)
		DIE(("Could not allocate WM hints\n"));
	xwmh->flags = (InputHint | StateHint | IconPixmapHint | 
				   IconWindowHint | IconPositionHint | IconMaskHint | 
				   WindowGroupHint);
	xwmh->input = False;
	xwmh->initial_state = settings.start_withdrawn ? WithdrawnState: NormalState;
	xwmh->icon_pixmap = None;
	xwmh->icon_x = 0;
	xwmh->icon_y = 0;
	xwmh->icon_mask = None;
	xwmh->window_group = settings.start_withdrawn ? tray_data.tray : None;
	xwmh->icon_window = settings.start_withdrawn ? tray_data.tray : None;

	if ((xch = XAllocClassHint()) == NULL)
		DIE(("Could not allocate class hints\n"));
	xch->res_class = PROGNAME;
	xch->res_name = PROGNAME;

	if ((xsh = XAllocSizeHints()) == NULL)
		DIE(("Could not allocate size hints\n"));
	*xsh = tray_data.xsh;
	xsh->flags = PWinGravity | PSize | PPosition;
	xsh->win_gravity = settings.geom_gravity;

	XSetWMProperties(tray_data.dpy, tray_data.tray, &wm_name, NULL, argv, argc, xsh, xwmh, xch);

	XFree(xch);
	XFree(xsh);
	XFree(xwmh);
	XFree((char *)wm_name.value);

	/* v0.2 tray protocol support */
	orient = settings.vertical ? _NET_SYSTEM_TRAY_ORIENTATION_HORZ : _NET_SYSTEM_TRAY_ORIENTATION_VERT;
	net_system_tray_orientation = XInternAtom(tray_data.dpy, TRAY_ORIENTATION_ATOM, False);
	XChangeProperty(tray_data.dpy, tray_data.tray, 
			net_system_tray_orientation, net_system_tray_orientation, 32, 
			PropModeReplace, 
			(unsigned char *) &orient, 1);

	if (settings.parent_bg)
		XSetWindowBackgroundPixmap(tray_data.dpy, tray_data.tray, ParentRelative);

	protocols_atoms[0] = tray_data.xa_wm_delete_window;
	protocols_atoms[1] = tray_data.xa_wm_take_focus;
	XSetWMProtocols(tray_data.dpy, tray_data.tray, protocols_atoms, 2);
	XSelectInput(tray_data.dpy, tray_data.tray, StructureNotifyMask | FocusChangeMask | PropertyChangeMask | ExposureMask );

	x11_extend_root_event_mask(tray_data.dpy, PropertyChangeMask);

#ifdef XPM_SUPPORTED
	if (settings.pixmap_bg) tray_init_pixmap_bg();
#endif
}

int tray_set_wm_hints()
{
	int mwm_decor = 0;

	if (settings.deco_flags & DECO_TITLE)
		mwm_decor |= MWM_DECOR_TITLE | MWM_DECOR_MENU;
	if (settings.deco_flags & DECO_BORDER)
		mwm_decor |= MWM_DECOR_RESIZEH | MWM_DECOR_BORDER;

	mwm_set_hints(tray_data.dpy, tray_data.tray, mwm_decor, MWM_FUNC_ALL);

	if (ewmh_check_support(tray_data.dpy)) {
		if (settings.sticky) {
			ewmh_add_window_state(tray_data.dpy, tray_data.tray, _NET_WM_STATE_STICKY);
			x11_send_client_msg32(tray_data.dpy, 
					DefaultRootWindow(tray_data.dpy), 
					tray_data.tray,
					XInternAtom(tray_data.dpy, "_NET_WM_DESKTOP", False),
					0xFFFFFFFF, /* all desktops */
					1, /* source indication: normal window */
					0,
					0,
					0);
		}

		if (settings.skip_taskbar)
			ewmh_add_window_state(tray_data.dpy, tray_data.tray, _NET_WM_STATE_SKIP_TASKBAR);
		
		if (settings.wnd_layer != NULL)
			ewmh_add_window_state(tray_data.dpy, tray_data.tray, settings.wnd_layer);

		/* TODO: CHECK this with compiz */
		if (strcmp(settings.wnd_type, _NET_WM_WINDOW_TYPE_NORMAL) == 0)
			ewmh_add_window_type(tray_data.dpy, tray_data.tray, settings.wnd_type);
		ewmh_add_window_type(tray_data.dpy, tray_data.tray, _NET_WM_WINDOW_TYPE_NORMAL);
	}

	return SUCCESS;
}

void tray_acquire_selection()
{
	static char *tray_sel_atom_name = NULL;
	Time timestamp;

	/* Obtain selection atom name basing on current screen number */	
	if (tray_sel_atom_name == NULL) {
		tray_sel_atom_name = (char *)malloc(strlen(TRAY_SEL_ATOM) + 10);
		if (tray_sel_atom_name == NULL) DIE(("out of memory\n"));
		snprintf(tray_sel_atom_name,
				strlen(TRAY_SEL_ATOM) + 10,
				"%s%u", 
				TRAY_SEL_ATOM, 
				DefaultScreen(tray_data.dpy));
	}

	DBG(8, ("tray_sel_atom_name=%s\n", tray_sel_atom_name));
	/* Initialize atom values */
	tray_data.xa_tray_selection = XInternAtom(tray_data.dpy, tray_sel_atom_name, False);
	tray_data.xa_tray_opcode = XInternAtom(tray_data.dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	tray_data.xa_tray_data = XInternAtom(tray_data.dpy, "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);

	timestamp = x11_get_server_timestamp(tray_data.dpy, tray_data.tray);
	/* Save old selection owner */
	tray_data.old_selection_owner = XGetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection);

	DBG(6, ("old selection owner: 0x%x\n", tray_data.old_selection_owner));

	/* Acquire selection */
	XSetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection, 
			tray_data.tray, timestamp);
	/* Check if we have really got the selection */
	if (XGetSelectionOwner(tray_data.dpy, tray_data.xa_tray_selection) != tray_data.tray) {
		DIE(("could not set selection owner.\nMay be another (greedy) tray running?\n"));
	} else {
		tray_data.is_active = True;
		DBG(0, ("ok, got _NET_SYSTEM_TRAY selection\n"));
	}
	/* Send the message notifying about new MANAGER */	
	x11_send_client_msg32(tray_data.dpy, DefaultRootWindow(tray_data.dpy),
	                      DefaultRootWindow(tray_data.dpy),
	                      XInternAtom(tray_data.dpy, "MANAGER", False),
	                      timestamp,
	                      tray_data.xa_tray_selection, tray_data.tray, 0, 0);	
}

void tray_show_window()
{
	tray_set_wm_hints();
	tray_update_window_size();
	XMapRaised(tray_data.dpy, tray_data.tray);
	XMoveWindow(tray_data.dpy, tray_data.tray, tray_data.xsh.x, tray_data.xsh.y);
	tray_set_wm_hints();
	tray_update_window_size();
}

