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

#define ERR(data)	{ print_msg data; }
#define DIE(data)	{ ERR(data); exit(-1); }

#endif
