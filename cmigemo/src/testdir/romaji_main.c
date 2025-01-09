/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * romaji_main.c -
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 * Last Change: 21-Sep-2004.
 */
/*
 * gcc -o romaji romaji_main.c ../romaji.c ../wordbuf.c
 */

#include <stdio.h>
#include <string.h>
#include "romaji.h"

#ifndef DICTDIR
# define DICTDIR "../dict"
#endif
#ifndef DICT_ROMA2HIRA
# define DICT_ROMA2HIRA	(DICTDIR "/roma2hira.dat")
#endif
#ifndef DICT_HIRA2KATA
# define DICT_HIRA2KATA	(DICTDIR "/hira2kata.dat")
#endif
#ifndef DICT_HAN2ZEN
# define DICT_HAN2ZEN	(DICTDIR "/han2zen.dat")
#endif
#ifndef DICT_ZEN2HAN
# define DICT_ZEN2HAN	(DICTDIR "/zen2han.dat")
#endif

    void
query_one(romaji* object, romaji* hira2kata, romaji* han2zen, romaji* zen2han, char* buf)
{
    unsigned char *stop;
    unsigned char *hira;
    unsigned char *zen;
    unsigned char *han;
    /* ローマ字→平仮名(表示)→片仮名(表示) */
    if (hira = romaji_convert(object, buf, &stop))
    {
	unsigned char* kata;

	printf("  hira=%s, stop=%s\n", hira, stop);
#if 1
	if (kata = romaji_convert2(hira2kata, hira, &stop, 0))
	{
	    printf("  kata=%s, stop=%s\n", kata, stop);
	    if (han = romaji_convert2(zen2han, kata, &stop, 0))
	    {
		printf("  han=%s, stop=%s\n", han, stop);
		romaji_release(zen2han, han);
	    }
	    romaji_release(hira2kata, kata);
	}
#endif
	romaji_release(object, hira);
    }
#if 1
    if (zen = romaji_convert2(han2zen, buf, &stop, 0))
    {
	printf("  zen=%s, stop=%s\n", zen, stop);
	romaji_release(han2zen, zen);
    }
#endif
    fflush(stdout);
}

    void
query_loop(romaji* object, romaji* hira2kata, romaji* han2zen, romaji* zen2han)
{
    char buf[256], *ans;

    while (1)
    {
	printf("QUERY: ");
	if (!fgets(buf, sizeof(buf), stdin))
	{
	    printf("\n");
	    break;
	}
	/* 改行をNUL文字に置き換える */
	if ((ans = strchr(buf, '\n')) != NULL)
	    *ans = '\0';
	query_one(object, hira2kata, han2zen, zen2han, buf);
    }
}

    int
main(int argc, char** argv)
{
    romaji *object, *hira2kata, *han2zen, *zen2han;
    char *word = NULL;

    object = romaji_open();
    hira2kata = romaji_open();
    han2zen = romaji_open();
    zen2han = romaji_open();
    romaji_set_verbose(zen2han, 1);

    while (*++argv)
    {
	if (0)
	    ;
	else if (argv[1] && (!strcmp("--word", *argv) || !strcmp("-w", *argv)))
	    word = *++argv;
    }

    if (object && hira2kata && han2zen && zen2han)
    {
	int retval = 0;

	retval = romaji_load(object, DICT_ROMA2HIRA);
	printf("romaji_load(%s)=%d\n", DICT_ROMA2HIRA, retval);
	retval = romaji_load(hira2kata, DICT_HIRA2KATA);
	printf("romaji_load(%s)=%d\n", DICT_HIRA2KATA, retval);
	retval = romaji_load(han2zen, DICT_HAN2ZEN);
	printf("romaji_load(%s)=%d\n", DICT_HAN2ZEN, retval);
	retval = romaji_load(zen2han, DICT_ZEN2HAN);
	printf("romaji_load(%s)=%d\n", DICT_HAN2ZEN, retval);
	if (word)
	    query_one(object, hira2kata, han2zen, zen2han, word);
	else
	    query_loop(object, hira2kata, han2zen, zen2han);
    }

    if (han2zen)
	romaji_close(han2zen);
    if (hira2kata)
	romaji_close(hira2kata);
    if (object)
	romaji_close(object);

    return 0;
}
