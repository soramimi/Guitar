/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * wordlist.h -
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 * Last Change: 20-Sep-2009.
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "wordlist.h"

int n_wordlist_open	= 0;
int n_wordlist_close	= 0;
int n_wordlist_total	= 0;

    wordlist_p
wordlist_open_len(const unsigned char* ptr, int len)
{
    if (ptr && len >= 0)
    {
	wordlist_p p;

	if ((p = (wordlist_p)malloc(sizeof(*p) + len + 1)) != NULL)
	{
	    p->ptr  = (char*)(p + 1);
	    p->next = NULL;
	    /*
	     * ほぼstrdup()と等価な実装。単語の保存に必要な総メモリを知りた
	     * いので独自に再実装。
	     */
	    memcpy(p->ptr, ptr, len);
	    p->ptr[len] = '\0';

	    ++n_wordlist_open;
	    n_wordlist_total += len;
	}
	return p;
    }
    return NULL;
}

    wordlist_p
wordlist_open(const unsigned char* ptr)
{
    wordlist_p p = NULL;
    if (ptr != NULL)
    {
        size_t len;
        len = strlen(ptr);
        p = wordlist_open_len(ptr, (len < INT_MAX ? (int)len : INT_MAX));
    }
    return p;
}

    void
wordlist_close(wordlist_p p)
{
    while (p)
    {
	wordlist_p next = p->next;

	++n_wordlist_close;
	free(p);
	p = next;
    }
}
