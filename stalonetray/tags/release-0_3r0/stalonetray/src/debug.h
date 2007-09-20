/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* icons.h
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

#define SUCCESS 1
#define FAILURE 0

#define MATCH 1
#define NO_MATCH 0

#define ERR(...)	{fprintf(stderr, "Error: "); fprintf(stderr, __VA_ARGS__);}
#define DIE(...)	{ ERR(__VA_ARGS__); exit(-1); }

#ifdef DEBUG
	#define DBG_PRN_TRAY_PREFIX	(1L << 0)
	#define DBG_PRN_DPY (1L << 1)
	#define DBG_PRN_TIMESTAMP (1L << 2)
	#define DBG_MASK	4

	extern int trace_mode;

	#define DBG(level, ...)		if (settings.dbg_level >= level || trace_mode) {      \
									if (DBG_MASK & DBG_PRN_TRAY_PREFIX) \
										fprintf(stderr, "[STALONETRAY] ");    \
									if (DBG_MASK & DBG_PRN_DPY) \
										fprintf(stderr, "(%s) ", DisplayString(tray_data.dpy));    \
									if (DBG_MASK & DBG_PRN_TIMESTAMP) {\
										time_t curtime = time(NULL);\
										struct tm * loctime = localtime(&curtime);\
                                        char *timestr;\
										int timestrlen;\
										timestrlen = strftime(NULL, 255, "%Y-%m-%d %H:%M:%S", loctime) + 1;\
										timestr = calloc(timestrlen, sizeof(char));\
										strftime(timestr, timestrlen, "%Y-%m-%d %H:%M:%S", loctime);\
                                        fprintf(stderr, "%s ", timestr); \
										free(timestr);\
									}\
									fprintf(stderr, "[%s] ", __func__); \
									fprintf(stderr, __VA_ARGS__);       \
								}
#else
	#define DBG(...)
#endif



#endif

