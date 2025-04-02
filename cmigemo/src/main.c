/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * main.c - migemo library test driver
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 * Last Change: 23-Feb-2004.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "migemo.h"

#define MIGEMO_ABOUT "cmigemo - C/Migemo Library " MIGEMO_VERSION " Driver"
#define MIGEMODICT_NAME "migemo-dict"
#define MIGEMO_SUBDICT_MAX 8

/*
 * main
 */

int query_loop(migemo *p, int quiet)
{
	while (!feof(stdin)) {
		unsigned char buf[256], *ans;

		if (!quiet)
			printf("QUERY: ");
		/* Was using gets() but changed to fgets() */
		if (!fgets(buf, sizeof(buf), stdin)) {
			if (!quiet)
				printf("\n");
			break;
		}
		/* Replace newline with NUL character */
		if ((ans = strchr(buf, '\n')) != NULL)
			*ans = '\0';

		ans = migemo_query(p, buf);
		if (ans)
			printf(quiet ? "%s\n" : "PATTERN: %s\n", ans);
		fflush(stdout);
		migemo_release(p, ans);
	}
	return 0;
}

static void
help(char *prgname)
{
	printf("\
%s \n\
\n\
USAGE: %s [OPTIONS]\n\
\n\
OPTIONS:\n\
  -d --dict <dict>	Use a file <dict> for dictionary.\n\
  -s --subdict <dict>	Sub dictionary files. (MAX %d times)\n\
  -q --quiet		Show no message except results.\n\
  -v --vim		Use vim style regexp.\n\
  -e --emacs		Use emacs style regexp.\n\
  -n --nonewline	Don't use newline match.\n\
  -w --word <word>	Expand a <word> and soon exit.\n\
  -h --help		Show this message.\n\
",
		MIGEMO_ABOUT, prgname, MIGEMO_SUBDICT_MAX);
	exit(0);
}

int main(int argc, char **argv)
{
	int mode_vim = 0;
	int mode_emacs = 0;
	int mode_nonewline = 0;
	int mode_quiet = 0;
	char *dict = NULL;
	char *subdict[MIGEMO_SUBDICT_MAX];
	int subdict_count = 0;
	migemo *pmigemo;
	FILE *fplog = stdout;
	char *word = NULL;
	char *prgname = argv[0];

	memset(subdict, 0, sizeof(subdict));
	while (*++argv) {
		if (0)
			;
		else if (!strcmp("--vim", *argv) || !strcmp("-v", *argv))
			mode_vim = 1;
		else if (!strcmp("--emacs", *argv) || !strcmp("-e", *argv))
			mode_emacs = 1;
		else if (!strcmp("--nonewline", *argv) || !strcmp("-n", *argv))
			mode_nonewline = 1;
		else if (argv[1] && (!strcmp("--dict", *argv) || !strcmp("-d", *argv)))
			dict = *++argv;
		else if (argv[1]
			&& (!strcmp("--subdict", *argv) || !strcmp("-s", *argv))
			&& subdict_count < MIGEMO_SUBDICT_MAX)
			subdict[subdict_count++] = *++argv;
		else if (argv[1] && (!strcmp("--word", *argv) || !strcmp("-w", *argv)))
			word = *++argv;
		else if (!strcmp("--quiet", *argv) || !strcmp("-q", *argv))
			mode_quiet = 1;
		else if (!strcmp("--help", *argv) || !strcmp("-h", *argv))
			help(prgname);
	}

#ifdef _PROFILE
	fplog = fopen("exe.log", "wt");
#endif

	/* Search for the dictionary in the current directory and one directory up */
	if (!dict) {
		pmigemo = migemo_open("./dict/" MIGEMODICT_NAME);
		if (!word && !mode_quiet) {
			fprintf(fplog, "migemo_open(\"%s\")=%p\n",
				"./dict/" MIGEMODICT_NAME, pmigemo);
		}
		if (!pmigemo || !migemo_is_enable(pmigemo)) {
			migemo_close(pmigemo); /* No problem even if NULL is closed */
			pmigemo = migemo_open("../dict/" MIGEMODICT_NAME);
			if (!word && !mode_quiet) {
				fprintf(fplog, "migemo_open(\"%s\")=%p\n",
					"../dict/" MIGEMODICT_NAME, pmigemo);
			}
		}
	} else {
		pmigemo = migemo_open(dict);
		if (!word && !mode_quiet)
			fprintf(fplog, "migemo_open(\"%s\")=%p\n", dict, pmigemo);
	}
	/* Load sub-dictionaries */
	if (subdict_count > 0) {
		int i;

		for (i = 0; i < subdict_count; ++i) {
			int result;

			if (subdict[i] == NULL || subdict[i][0] == '\0')
				continue;
			result = migemo_load(pmigemo, MIGEMO_DICTID_MIGEMO, subdict[i]);
			if (!word && !mode_quiet)
				fprintf(fplog, "migemo_load(%p, %d, \"%s\")=%d\n",
					pmigemo, MIGEMO_DICTID_MIGEMO, subdict[i], result);
		}
	}

	if (!pmigemo)
		return 1;
	else {
		if (mode_vim) {
			migemo_set_operator(pmigemo, MIGEMO_OPINDEX_OR, "\\|");
			migemo_set_operator(pmigemo, MIGEMO_OPINDEX_NEST_IN, "\\%(");
			migemo_set_operator(pmigemo, MIGEMO_OPINDEX_NEST_OUT, "\\)");
			if (!mode_nonewline)
				migemo_set_operator(pmigemo, MIGEMO_OPINDEX_NEWLINE, "\\_s*");
		} else if (mode_emacs) {
			migemo_set_operator(pmigemo, MIGEMO_OPINDEX_OR, "\\|");
			migemo_set_operator(pmigemo, MIGEMO_OPINDEX_NEST_IN, "\\(");
			migemo_set_operator(pmigemo, MIGEMO_OPINDEX_NEST_OUT, "\\)");
			if (!mode_nonewline)
				migemo_set_operator(pmigemo, MIGEMO_OPINDEX_NEWLINE, "\\s-*");
		}
#ifndef _PROFILE
		if (word) {
			unsigned char *ans;

			ans = migemo_query(pmigemo, word);
			if (ans)
				fprintf(fplog, mode_vim ? "%s" : "%s\n", ans);
			migemo_release(pmigemo, ans);
		} else {
			if (!mode_quiet)
				printf("clock()=%f\n", (float)clock() / CLOCKS_PER_SEC);
			query_loop(pmigemo, mode_quiet);
		}
#else
		/* プロファイル用 */
		{
			unsigned char *ans;

			ans = migemo_query(pmigemo, "a");
			if (ans)
				fprintf(fplog, "  [%s]\n", ans);
			migemo_release(pmigemo, ans);

			ans = migemo_query(pmigemo, "k");
			if (ans)
				fprintf(fplog, "  [%s]\n", ans);
			migemo_release(pmigemo, ans);
		}
#endif
		migemo_close(pmigemo);
	}

	if (fplog != stdout)
		fclose(fplog);
	return 0;
}
