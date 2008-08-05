/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* debug.c
* Mon, 01 May 2006 01:44:42 +0700
* -------------------------------
* Debugging code/utilities 
* -------------------------------*/

#include "config.h"
#include "debug.h"
#include "xutils.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>

#ifdef DBG_PRINT_PID
#include <sys/types.h>
#include <unistd.h>
#endif

/* Print the message to STDERR (varadic macros is not used in the sake of portability) */
void print_message_to_stderr(const char *fmt,...)
{
	static char msg[PATH_MAX];
	va_list va;
	va_start(va, fmt);
	vsnprintf(msg, PATH_MAX, fmt, va);
	va_end(va);
	fprintf(stderr, msg);
}

#ifdef DEBUG

int trace_mode = False;

void print_debug_header(const char *funcname, const char *fname, const int line, const int level)
{
	static pid_t pid = 0;

#ifdef DBG_PRINT_TRAY_PREFIX
	fprintf(stderr, "[STALONETRAY] ");
#endif
#ifdef DBG_PRINT_PID
	if (!pid) pid = getpid();
	fprintf(stderr, "%d ", pid);
#endif
#ifdef DBG_PRINT_DPY
	if (tray_data.dpy != NULL)
		fprintf(stderr, "(%s) ", DisplayString(tray_data.dpy));
	else
		fprintf(stderr, "(dpy) ");
#endif
#ifdef DBG_PRINT_TIMESTAMP
	{
        static char timestr[PATH_MAX+1];
		time_t curtime = time(NULL);
		struct tm * loctime = localtime(&curtime);
		strftime(timestr, PATH_MAX, "%H:%M:%S", loctime);
		fprintf(stderr, "%s ", timestr); 
	}
#endif
	fprintf(stderr, "[%d] ", level);
#ifdef DBG_PRINT_LOCATION
	fprintf(stderr, "(%s:%d) ", fname, line);
#endif
	fprintf(stderr, "%s(): ", funcname); 
}
#endif


/* Print the summary of icon data */
int print_icon_data(struct TrayIcon *ti)
#ifdef DEBUG
/* Debug version */
{
	XWindowAttributes xwa;
	Window real_parent, root, *children = NULL;
	unsigned int nchildren = 0;
	int rc;

	DBG(6, ("ptr = %p\n", ti));
	DBG(6, ("  prev = %p\n", ti->prev));
	DBG(6, ("  next = %p\n", ti->next));
	DBG(6, ("  wid = 0x%x\n", ti->wid));
	DBG(6, ("  invalid = %d\n", ti->is_invalid));
	DBG(6, ("  layed_out = %d\n", ti->is_layed_out));
	DBG(6, ("  update_pos = %d\n", ti->is_updated));
	DBG(6, ("  visible = %d\n", ti->is_visible));
	DBG(6, ("  grd_rect = %dx%d+%d+%d\n", 
			ti->l.grd_rect.w, ti->l.grd_rect.h,
			ti->l.grd_rect.x, ti->l.grd_rect.y));
	DBG(6, ("  icn_rect = %dx%d+%d+%d\n", 
			ti->l.icn_rect.w, ti->l.icn_rect.h,
			ti->l.icn_rect.x, ti->l.icn_rect.y));
	DBG(6, ("  wnd_sz = %dx%d\n", ti->l.wnd_sz.x, ti->l.wnd_sz.y));
	DBG(6, ("  cmode = %d\n", ti->cmode));
	DBG(6, ("  xembed_supported = %d\n", ti->is_xembed_supported));
	DBG(6, ("  xembed_accepts_focus = %d\n", ti->is_xembed_accepts_focus));
	DBG(6, ("  xembed_last_timestamp = %d\n", ti->xembed_last_timestamp));
	DBG(6, ("  xembed_last_msgid = %d\n", ti->xembed_last_msgid));

	rc = XQueryTree(tray_data.dpy, ti->wid, &root, &real_parent, &children, &nchildren);
	if (rc && children != NULL && nchildren > 0) XFree(children);
	if (!rc) {
		DBG(6, ("  ERR:could not get real mid-parent\n"));
	} else {
		DBG(6, ("  real parent wid: 0x%x\n", real_parent));
	}
	if (ti->is_embedded) {
		if (!XGetWindowAttributes(tray_data.dpy, ti->mid_parent, &xwa)) {
			DBG(6, ("  ERR: could not get mid-parents stats\n"));
		} else {
			DBG(6, ("  mid-parent wid:      0x%x\n", ti->mid_parent));
			DBG(6, ("  mid-parent state:    %s\n",  xwa.map_state == IsUnmapped ? "Unmapped" : (xwa.map_state == IsUnviewable ? "Unviewable" : "Viewable")));
			DBG(6, ("  mid-parent geometry: %dx%d+%d+%d\n", xwa.width, xwa.height, xwa.x, xwa.y));
		}
	} else {
		DBG(6, ("  not embedded\n"));
	}
	if (!XGetWindowAttributes(tray_data.dpy, ti->wid, &xwa)) {
		DBG(6, ("  ERR: could not get icon window attributes\n"));
	} else {
		DBG(6, ("  geometry: %dx%d+%d+%d\n", xwa.width, xwa.height, xwa.x, xwa.y));
		DBG(6, ("  mapstate: %s\n", xwa.map_state == IsUnmapped ? "Unmapped" : (xwa.map_state == IsUnviewable ? "Unviewable" : "Viewable")));
	}

	/* This resets x11 error state (which might have left from X11 calls above) */
	x11_ok();

	return NO_MATCH;
}
#else
/* Non-debug version (less verbose) */
{
	fprintf(stderr, "ptr = %p\n", ti);
	fprintf(stderr, "\tprev = %p\n", ti->prev);
	fprintf(stderr, "\tnext = %p\n\n", ti->next);
	fprintf(stderr, "\twid = 0x%x\n\n", ti->wid);
	fprintf(stderr, "\tinvalid = %d\n", ti->is_invalid);
	fprintf(stderr, "\tsupports_xembed = %d\n", ti->is_xembed_supported);
	fprintf(stderr, "\tlayed_out = %d\n", ti->is_layed_out);
	fprintf(stderr, "\tembedded = %d\n", ti->is_embedded);
	fprintf(stderr, "\tvisible = %d\n", ti->is_visible);
	fprintf(stderr, "\tupdate_pos = %d\n", ti->is_updated);
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

void dump_icon_list()
{
	DBG(4, ("====================\n"));
	DBG(4, ("here comes icons list\n"));
	DBG(4, ("====================\n"));
	icon_list_forall(&print_icon_data);
	DBG(4, ("====================\n"));
	DBG(4, ("====================\n"));
}
