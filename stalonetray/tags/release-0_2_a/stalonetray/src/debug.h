/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* icons.h
* Tue, 24 Aug 2004 12:05:38 +0700
* -------------------------------
* Debugging code
* -------------------------------*/

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>

#include "config.h"
#include "settings.h"

#define ERR(...)	{fprintf(stderr, "Error: "); fprintf(stderr, __VA_ARGS__);}
#define DIE(...)	{ ERR(__VA_ARGS__); exit(-1); }

#ifdef DEBUG
	#define DBG_PRN_TRAY_PREFIX	(1L << 0)
	#define DBG_PRN_DPY_PREFIX (1L << 1)
	#define DBG_MASK	0

	#define DBG(level, ...)		if (settings.dbg_level >= level) {      \
									if (DBG_MASK & DBG_PRN_TRAY_PREFIX) \
										fprintf(stderr, "[STRAY] ");    \
									fprintf(stderr, "[%s] ", __func__); \
									fprintf(stderr, __VA_ARGS__);       \
								}
#else
	#define DBG(...)
#endif

#endif

