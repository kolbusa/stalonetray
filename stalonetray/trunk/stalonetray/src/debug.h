/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* debug.h
* Tue, 24 Aug 2004 12:05:38 +0700
* -------------------------------
* Debugging code/utilities 
* -------------------------------*/

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "settings.h"
#include "tray.h"
#include "common.h"

#include <string.h>
#include <time.h>

/* Print the message to STDERR (in the sake of portability, we do not use varadic macros) */
void print_message_to_stderr(const char *fmt,...);

#ifdef DEBUG
/* The following defines control what is printed in each logged line */	
/* Print "stalonetray" */
#undef  DBG_PRINT_TRAY_PREFIX
/* Print the id of the process */
#define DBG_PRINT_PID
/* Print the name of display */
#define  DBG_PRINT_DPY
/* Print the timestamp */
#define DBG_PRINT_TIMESTAMP
/* Print the name of a function, file and line which outputs the message */
#undef DBG_PRINT_LOCATION

/* Print the debug header as specified by defines below */
void print_debug_header(const char *funcname, const char *fname, const int line, const int level);

/* If trace_mode == True, print all the messages regardless of their debug level */
extern int trace_mode;

/* Print the debug message of specified level */
#define DBG(level,message)	do { \
								if (settings.dbg_level >= level || trace_mode) { \
									print_debug_header(__FUNC__, __FILE__, __LINE__, level); \
									print_message_to_stderr message; \
									}; \
							} while (0)
#else /* DEBUG */
	/* Dummy declaration */
	#define DBG(level,message)	do {} while(0)
#endif /* DEBUG */

/* Print the summary of icon data */
int print_icon_data(struct TrayIcon *ti);
/* Print icon list contents */
void dump_icon_list();

#endif

