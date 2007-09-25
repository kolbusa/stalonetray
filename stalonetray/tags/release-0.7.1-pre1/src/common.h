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

#define PROGNAME PACKAGE

/* Default icon size */
#define FALLBACK_ICON_SIZE	24
/* Minimal icon size */
#define MIN_ICON_SIZE 16
/* Default KDE icon size */
#define KDE_ICON_SIZE 22

/* Meaningful names for return values */
#define SUCCESS 1
#define FAILURE 0

/* Meaningful names for return values of icon mass-operations */
#define MATCH 1
#define NO_MATCH 0

/* Portable macro for function name */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
	#define __FUNC__ __func__
#elif defined(__GNUC__) && __GNUC__ >= 3
	#define __FUNC__ __FUNCTION__
#else
	#define __FUNC__ "unknown"
#endif

/* Print a error message */
#ifdef DEBUG
	#define ERR(message)	DBG(0, message)
#else
	#define ERR(message)	print_message_to_stderr message
#endif

/* Print a message and... DIE */
#define DIE(message)		do { ERR(message); exit(-1); } while(0)

/* WARNING: feed following macro only with 
 * side-effects-free expressions */

/* Get a value within target interval */
#define cutoff(tgt,min,max) (tgt) < (min) ? (min) : ((tgt) > (max) ? max : tgt)

/* Update value to fit into target interval */
#define val_range(tgt,min,max) (tgt) = cutoff(tgt,min,max)

#endif
