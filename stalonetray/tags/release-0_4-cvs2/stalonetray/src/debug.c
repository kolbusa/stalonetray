/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* debug.c
* Mon, 01 May 2006 01:44:42 +0700
* -------------------------------
* Debugging code/utilities 
* -------------------------------*/

#include "config.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>

void print_msg(const char *fmt,...)
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

void print_debug_header(const char *funcname)
{
	if ((DBG_MASK) & (DBG_PRN_TRAY_PREFIX))
		fprintf(stderr, "[STALONETRAY] ");
	
    if ((DBG_MASK) & (DBG_PRN_DPY) && tray_data.dpy != NULL)
        fprintf(stderr, "(%s) ", DisplayString(tray_data.dpy));
		
	if ((DBG_MASK) & (DBG_PRN_TIMESTAMP)) {
        static char timestr[PATH_MAX+1];
		time_t curtime = time(NULL);
		struct tm * loctime = localtime(&curtime);
		
		strftime(timestr, PATH_MAX, "%Y-%m-%d %H:%M:%S", loctime);
		fprintf(stderr, "%s ", timestr); 
	}

	fprintf(stderr, "[%s] ", funcname); 
}

#endif

