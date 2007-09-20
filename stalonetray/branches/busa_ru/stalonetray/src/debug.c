/*
 * Misc. debug routines --- messages\etc.
 */

#include <stdlib.h>
#include <stdio.h>

#include "debug.h"


void die(char *msg)
{
	fprintf(stderr, "%s", msg);
	exit(1);
}
