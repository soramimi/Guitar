/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * profile_speed.c - スピード計測
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 * Last Change: 20-Jun-2004.
 */

#define NUM_TRIAL 10

#include <stdio.h>
#include "migemo.h"

#ifndef DICTDIR
# define DICTDIR "../dict"
#endif

    int
main(int argc, char** argv)
{
    migemo* pmig;
    char* ans;
    char key[2] = { '\0', '\0' };
    int i;

    printf("Start\n");
    pmig = migemo_open(DICTDIR "/migemo-dict");
    printf("Loaded\n");
    if (pmig != NULL)
    {
	for (i = 0; i < NUM_TRIAL; ++i)
	{
	    printf("[%d] Progress... ", i);
	    for (key[0] = 'a'; key[0] <= 'z'; ++key[0])
	    {
		printf("%s", key);
		fflush(stdout);
		ans = migemo_query(pmig, key);
		migemo_release(pmig, ans);
	    }
	    printf("\n");
	}
	migemo_close(pmig);
    }
    return 0;
}
