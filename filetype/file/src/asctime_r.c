/*	$File$	*/

#include "file.h"
#ifndef	lint
FILE_RCSID("@(#)$File: ascmagic.c,v 1.84 2011/12/08 12:38:24 rrt Exp $")
#endif	/* lint */
#include <time.h>
#include <string.h>

/* asctime_r is not thread-safe anyway */
char *
asctime_r(const struct tm *t, char *dst)
{
	char *p = asctime(t);
	if (p == NULL)
		return NULL;
	memcpy(dst, p, 26);
	return dst;
}
