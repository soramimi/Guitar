#include "IncrementalSearch.h"

/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * main.c - migemoライブラリテストドライバ
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 * Last Change: 23-Feb-2004.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <QDebug>

#include "migemo.h"

#define MIGEMO_ABOUT "cmigemo - C/Migemo Library " MIGEMO_VERSION " Driver"
#define MIGEMODICT_NAME "migemo-dict"
#define MIGEMO_SUBDICT_MAX 8

struct IncrementalSearch::M {
	migemo *pmigemo = nullptr;
	std::string dict_path;
};

IncrementalSearch::IncrementalSearch()
	: m(new M)
{
	// m->dict_path = "/home/soramimi/develop/Guitar/cmigemo/dict/utf-8.d/migemo-dict";
	m->dict_path = "../misc/migemo/migemo-dict";
}

IncrementalSearch::~IncrementalSearch()
{
	if (m->pmigemo) {
		migemo_close(m->pmigemo);
	}
	delete m;
}

bool IncrementalSearch::open()
{
	if (!m->pmigemo) {
		m->pmigemo = migemo_open(m->dict_path.c_str());
		if (!m->pmigemo) return false;

		char *subdict[MIGEMO_SUBDICT_MAX];
		int subdict_count = 0;

		memset(subdict, 0, sizeof(subdict));

		/* サブ辞書を読み込む */
		if (subdict_count > 0) {
			for (int i = 0; i < subdict_count; ++i) {
				if (subdict[i] == NULL || subdict[i][0] == '\0') continue;
				migemo_load(m->pmigemo, MIGEMO_DICTID_MIGEMO, subdict[i]);
			}
		}
	}
	return true;
}

std::string IncrementalSearch::query(char const *word)
{
	std::string ret;
	if (m->pmigemo) {
		unsigned char *ans = (unsigned char *)migemo_query(m->pmigemo, (unsigned char const *)word);
		if (ans) {
			ret = (char const *)ans;
		}
		migemo_release(m->pmigemo, ans);
	}
	return ret;
}
