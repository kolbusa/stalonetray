/* vim:tabstop=4
 * Misc. debug routines --- messages\etc.
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>

void die(char *msg);

#ifdef DEBUG
	#define DBG_LEVEL	10
	#define DBG_CLS_ICONS	(1L << 0)
	#define DBG_CLS_XEMBED	(1L << 1)
	#define DBG_CLS_MAIN	(1L << 2)
	#define DBG_CLS_EVENT	(1L << 3)
	#define DBG_PRN_TRAY_PREFIX	(1L << 4)
	#define DBG_PRN_CLS_PREFIX (1L << 5)
	#define DBG_MASK	(DBG_CLS_MAIN | DBG_PRN_CLS_PREFIX | DBG_CLS_ICONS | DBG_CLS_EVENT)

	#define debugmsg(dbg_class,dbg_level, ...)	if ((DBG_MASK & dbg_class) && dbg_level <= DBG_LEVEL) {\
													if (DBG_MASK & DBG_PRN_TRAY_PREFIX)\
														fprintf(stderr, "[STRAY]\t");\
													if (DBG_MASK & DBG_PRN_CLS_PREFIX) {\
														switch (dbg_class) { \
														case DBG_CLS_MAIN: \
															fprintf(stderr, "[MAIN]\t"); \
															break;\
														case DBG_CLS_ICONS:\
															fprintf(stderr, "[ICONS]\t"); \
															break;\
														case DBG_CLS_XEMBED:\
															fprintf(stderr, "[XEMBED]\t"); \
															break; \
														case DBG_CLS_EVENT:\
															fprintf(stderr, "[EVENT]\t"); \
															break; \
														} \
													} \
													fprintf(stderr, __VA_ARGS__);\
												}
			
#else
	#define debugmsg(...)
#endif

#endif

