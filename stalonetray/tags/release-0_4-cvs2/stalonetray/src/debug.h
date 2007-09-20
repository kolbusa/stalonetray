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

#include <string.h>
#include <time.h>

void print_msg(const char *fmt,...);

#ifdef DEBUG
	#define DBG_PRN_TRAY_PREFIX	(1L << 0)
	#define DBG_PRN_DPY (1L << 1)
	#define DBG_PRN_TIMESTAMP (1L << 2)
	#define DBG_MASK (DBG_PRN_TIMESTAMP)

	void print_debug_header(const char *funcname);

	extern int trace_mode;

	#define DBG(level,data)		do { \
	                            	if (settings.dbg_level >= level || trace_mode) { \
	                            		print_debug_header(__func__); \
	                            		print_msg data; \
	                            		}; \
	                            } while (0)
#else
	#define DBG(level, data)	do {} while(0)
#endif

#endif

