/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* common.h
* Mon, 01 May 2006 01:45:08 +0700
* -------------------------------
* Common declarations
* -------------------------------*/

#ifndef _COMMON_H_
#define _COMMON_H_

#include "debug.h"

#define SUCCESS 1
#define FAILURE 0

#define MATCH 1
#define NO_MATCH 0

#ifdef DEBUG
	#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
		#define PORT_FUNC __func__
	#elif defined(__GNUC__) && __GNUC__ >= 3
		#define PORT_FUNC __FUNCTION__
	#else
		#define PORT_FUNC "unknown"
	#endif
#endif

#ifdef DEBUG
	#define ERR(data)	DBG(0, data)
#else
	#define ERR(data)	do { print_msg data; } while(0)
#endif

#define DIE(data)	do { ERR(data); exit(-1); } while(0)

#endif
