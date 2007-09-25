/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* debug.c
* Mon, 01 May 2006 01:44:42 +0700
* -------------------------------
* Debugging code/utilities 
* -------------------------------*/

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>

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

