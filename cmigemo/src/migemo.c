/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * migemo.c -
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "wordbuf.h"
#include "wordlist.h"
#include "mnode.h"
#include "rxgen.h"
#include "romaji.h"
#include "filename.h"
#include "charset.h"
#include "migemo.h"

#define DICT_MIGEMO "migemo-dict"
#define DICT_ROMA2HIRA "roma2hira.dat"
#define DICT_HIRA2KATA "hira2kata.dat"
#define DICT_HAN2ZEN "han2zen.dat"
#define DICT_ZEN2HAN "zen2han.dat"
#define BUFLEN_DETECT_CHARSET 4096

#ifdef __BORLANDC__
# define EXPORTS __declspec(dllexport)
#else
# define EXPORTS
#endif

typedef int (*MIGEMO_PROC_ADDWORD)(void* data, unsigned char* word);

/* migemoオブジェクト */
struct _migemo
{
    int enable;
    mtree_p mtree;
    int charset;
    romaji* roma2hira;
    romaji* hira2kata;
    romaji* han2zen;
    romaji* zen2han;
    rxgen* rx;
    MIGEMO_PROC_ADDWORD addword;
    CHARSET_PROC_CHAR2INT char2int;
};

static const unsigned char VOWEL_CHARS[] = "aiueo";

    static int
my_strlen(const char* s)
{
    size_t len;

    len = strlen(s);
    return len <= INT_MAX ? (int)len : INT_MAX;
}

    static mtree_p
load_mtree_dictionary(mtree_p mtree, const char* dict_file)
{
    FILE *fp;

    if ((fp = fopen(dict_file, "rt")) == NULL)
	return NULL;			/* Can't find file */
    mtree = mnode_load(mtree, fp);
    fclose(fp);
    return mtree;
}

    static mtree_p
load_mtree_dictionary2(migemo* obj, const char* dict_file)
{
    if (obj->charset == CHARSET_NONE)
    {
	/* 辞書の文字セットにあわせて正規表現生成時の関数を変更する */
	CHARSET_PROC_CHAR2INT char2int = NULL;
	CHARSET_PROC_INT2CHAR int2char = NULL;
	obj->charset = charset_detect_file(dict_file);
	charset_getproc(obj->charset, &char2int, &int2char);
	if (char2int)
	{
	    migemo_setproc_char2int(obj, (MIGEMO_PROC_CHAR2INT)char2int);
	    obj->char2int = char2int;
	}
	if (int2char)
	    migemo_setproc_int2char(obj, (MIGEMO_PROC_INT2CHAR)int2char);
    }
    return load_mtree_dictionary(obj->mtree, dict_file);
}

    static void
dircat(char* buf, const char* dir, const char* file)
{
    strcpy(buf, dir);
    strcat(buf, "/");
    strcat(buf, file);
}

/*
 * migemo interfaces
 */

/**
 * Migemoオブジェクトに辞書、またはデータファイルを追加読み込みする。
 * dict_fileは読み込むファイル名を指定する。dict_idは読み込む辞書・データの
 * 種類を指定するもので以下のうちどれか一つを指定する:
 *
 *  <dl>
 *  <dt>MIGEMO_DICTID_MIGEMO</dt>
 *	<dd>mikgemo-dict辞書</dd>
 *  <dt>MIGEMO_DICTID_ROMA2HIRA</dt>
 *	<dd>ローマ字→平仮名変換表</dd>
 *  <dt>MIGEMO_DICTID_HIRA2KATA</dt>
 *	<dd>平仮名→カタカナ変換表</dd>
 *  <dt>MIGEMO_DICTID_HAN2ZEN</dt>
 *	<dd>半角→全角変換表</dd>
 *  <dt>MIGEMO_DICTID_ZEN2HAN</dt>
 *	<dd>全角→半角変換表</dd>
 *  </dl>
 *
 *  戻り値は実際に読み込んだ種類を示し、上記の他に読み込みに失敗したことを示す
 *  次の価が返ることがある。
 *
 *  <dl><dt>MIGEMO_DICTID_INVALID</dt></dl>
 * @param obj Migemoオブジェクト
 * @param dict_id 辞書ファイルの種類
 * @param dict_file 辞書ファイルのパス
 */
    EXPORTS int MIGEMO_CALLTYPE
migemo_load(migemo* obj, int dict_id, const char* dict_file)
{
    if (!obj && dict_file)
	return MIGEMO_DICTID_INVALID;

    if (dict_id == MIGEMO_DICTID_MIGEMO)
    {
	/* migemo辞書読み込み */
	mtree_p mtree;

	if ((mtree = load_mtree_dictionary2(obj, dict_file)) == NULL)
	    return MIGEMO_DICTID_INVALID;
	obj->mtree = mtree;
	obj->enable = 1;
	return dict_id;			/* Loaded successfully */
    }
    else
    {
	romaji *dict;

	switch (dict_id)
	{
	    case MIGEMO_DICTID_ROMA2HIRA:
		/* ローマ字辞書読み込み */
		dict = obj->roma2hira;
		break;
	    case MIGEMO_DICTID_HIRA2KATA:
		/* カタカナ辞書読み込み */
		dict = obj->hira2kata;
		break;
	    case MIGEMO_DICTID_HAN2ZEN:
		/* 半角→全角辞書読み込み */
		dict = obj->han2zen;
		break;
	    case MIGEMO_DICTID_ZEN2HAN:
		/* 半角→全角辞書読み込み */
		dict = obj->zen2han;
		break;
	    default:
		dict = NULL;
		break;
	}
	if (dict && romaji_load(dict, dict_file) == 0)
	    return dict_id;
	else
	    return MIGEMO_DICTID_INVALID;
    }
}

/**
 * Migemoオブジェクトを作成する。作成に成功するとオブジェクトが戻り値として
 * 返り、失敗するとNULLが返る。dictで指定したファイルがmigemo-dict辞書として
 * オブジェクト作成時に読み込まれる。辞書と同じディレクトリに:
 *
 *  <dl>
 *  <dt>roma2hira.dat</dt>
 *	<dd>ローマ字→平仮名変換表 </dd>
 *  <dt>hira2kata.dat</dt>
 *	<dd>平仮名→カタカナ変換表 </dd>
 *  <dt>han2zen.dat</dt>
 *	<dd>半角→全角変換表 </dd>
 *  </dl>
 *
 * という名前のファイルが存在すれば、存在したものだけが読み込まれる。dictに
 * NULLを指定した場合には、辞書を含めていかなるファイルも読み込まれない。
 * ファイルはオブジェクト作成後にもmigemo_load()関数を使用することで追加読み
 * 込みができる。
 * @param dict migemo-dict辞書のパス。NULLの時は辞書を読み込まない。
 * @returns 作成されたMigemoオブジェクト
 */
    EXPORTS migemo* MIGEMO_CALLTYPE
migemo_open(const char* dict)
{
    migemo *obj;

    /* migemoオブジェクトと各メンバを構築 */
    if (!(obj = (migemo*)calloc(1, sizeof(migemo))))
	return obj;
    obj->enable = 0;
    obj->mtree = mnode_open(NULL);
    obj->charset = CHARSET_NONE;
    obj->rx = rxgen_open();
    obj->roma2hira =	romaji_open();
    obj->hira2kata =	romaji_open();
    obj->han2zen =	romaji_open();
    obj->zen2han =	romaji_open();
    if (!obj->rx || !obj->roma2hira || !obj->hira2kata || !obj->han2zen
	    || !obj->zen2han)
    {
	migemo_close(obj);
	return obj = NULL;
    }

    /* デフォルトmigemo辞書が指定されていたらローマ字とカタカナ辞書も探す */
    if (dict)
    {
#ifndef _MAX_PATH
# define _MAX_PATH 1024 /* いい加減な数値 */
#endif
	char dir[_MAX_PATH];
	char roma_dict[_MAX_PATH];
	char kata_dict[_MAX_PATH];
	char h2z_dict[_MAX_PATH];
	char z2h_dict[_MAX_PATH];
	const char *tmp;
	mtree_p mtree;

	filename_directory(dir, dict);
	tmp = strlen(dir) ? dir : ".";
	dircat(roma_dict, tmp, DICT_ROMA2HIRA);
	dircat(kata_dict, tmp, DICT_HIRA2KATA);
	dircat(h2z_dict,  tmp, DICT_HAN2ZEN);
	dircat(z2h_dict,  tmp, DICT_ZEN2HAN);

	mtree = load_mtree_dictionary2(obj, dict);
	if (mtree)
	{
	    obj->mtree = mtree;
	    obj->enable = 1;
	    romaji_load(obj->roma2hira, roma_dict);
	    romaji_load(obj->hira2kata, kata_dict);
	    romaji_load(obj->han2zen, h2z_dict);
	    romaji_load(obj->zen2han, z2h_dict);
	}
    }
    return obj;
}

/**
 * Migemoオブジェクトを破棄し、使用していたリソースを解放する。
 * @param obj 破棄するMigemoオブジェクト
 */
    EXPORTS void MIGEMO_CALLTYPE
migemo_close(migemo* obj)
{
    if (obj)
    {
	if (obj->zen2han)
	    romaji_close(obj->zen2han);
	if (obj->han2zen)
	    romaji_close(obj->han2zen);
	if (obj->hira2kata)
	    romaji_close(obj->hira2kata);
	if (obj->roma2hira)
	    romaji_close(obj->roma2hira);
	if (obj->rx)
	    rxgen_close(obj->rx);
	if (obj->mtree)
	    mnode_close(obj->mtree);
	free(obj);
    }
}

/*
 * query version 2
 */

/*
 * mnodeの持つ単語リストを正規表現生成エンジンに入力する。
 */
    static void
migemo_query_proc(mnode* p, void* data)
{
    migemo *object = (migemo*)data;
    wordlist_p list = p->list;

    for (; list; list = list->next)
	object->addword(object, list->ptr);
}

/*
 * バッファを用意してmnodeに再帰で書き込ませる
 */
    static void
add_mnode_query(migemo* object, unsigned char* query)
{
    mnode *pnode;

    if ((pnode = mnode_query(object->mtree, query)) != NULL)
	mnode_traverse(pnode, migemo_query_proc, object);
}

/**
 * 入力をローマから仮名に変換して検索キーに加える。
 */
    static int
add_roma(migemo* object, unsigned char* query)
{
    unsigned char *stop, *hira, *kata, *han;

    hira = romaji_convert(object->roma2hira, query, &stop);
    if (!stop)
    {
	object->addword(object, hira);
	/* 平仮名による辞書引き */
	add_mnode_query(object, hira);
	/* 片仮名文字列を生成し候補に加える */
	kata = romaji_convert2(object->hira2kata, hira, NULL, 0);
	object->addword(object, kata);
	/* TODO: 半角カナを生成し候補に加える */
#if 1
	han = romaji_convert2(object->zen2han, kata, NULL, 0);
	object->addword(object, han);
	/*printf("kata=%s\nhan=%s\n", kata, han);*/
	romaji_release(object->zen2han, han);
#endif
	/* カタカナによる辞書引き */
	add_mnode_query(object, kata);
	romaji_release(object->hira2kata, kata); /* カタカナ解放 */
    }
    romaji_release(object->roma2hira, hira); /* 平仮名解放 */

    return stop ? 1 : 0;
}

/**
 * ローマ字の末尾に母音を付け加えて、各々を検索キーに加える。
 */
    static void
add_dubious_vowels(migemo* object, unsigned char* buf, int index)
{
    const unsigned char* ptr;
    for (ptr = VOWEL_CHARS; *ptr; ++ptr)
    {
	buf[index] = *ptr;
	add_roma(object, buf);
    }
}

/*
 * ローマ字変換が不完全だった時に、[aiueo]および"xn"と"xtu"を補って変換して
 * みる。
 */
    static void
add_dubious_roma(migemo* object, rxgen* rx, unsigned char* query)
{
    int max;
    int len;
    char *buf;

    if (!(len = my_strlen(query)))
	return;
    /*
     * ローマ字の末尾のアレンジのためのバッファを確保する。
     *	    内訳: オリジナルの長さ、NUL、吃音(xtu)、補足母音([aieuo])
     */
    max = len + 1 + 3 + 1;
    buf = malloc(max);
    if (buf == NULL)
	return;
    memcpy(buf, query, len);
    memset(&buf[len], 0, max - len);

    if (!strchr(VOWEL_CHARS, buf[len - 1]))
    {
	add_dubious_vowels(object, buf, len);
	/* 未確定単語の長さが2未満か、未確定文字の直前が母音ならば… */
	if (len < 2 || strchr(VOWEL_CHARS, buf[len - 2]))
	{
	    if (buf[len - 1] == 'n')
	    {
		/* 「ん」を補ってみる */
		memcpy(&buf[len - 1], "xn", 2);
		add_roma(object, buf);
	    }
	    else
	    {
		/* 「っ{元の子音}{母音}」を補ってみる */
		buf[len + 2] = buf[len - 1];
		memcpy(&buf[len - 1], "xtu", 3);
		add_dubious_vowels(object, buf, len + 3);
	    }
	}
    }

    free(buf);
}

/*
 * queryを文節に分解する。文節の切れ目は通常アルファベットの大文字。文節が複
 * 数文字の大文字で始まった文節は非大文字を区切りとする。
 */
    static wordlist_p
parse_query(migemo* object, const unsigned char* query)
{
    const unsigned char *curr = query;
    const unsigned char *start = NULL;
    wordlist_p querylist = NULL, *pp = &querylist;

    while (1)
    {
	int len, upper;
        int sum = 0;

	if (!object->char2int || (len = object->char2int(curr, NULL)) < 1)
	    len = 1;
	start = curr;
	upper = (len == 1 && isupper(*curr) && isupper(curr[1]));
	curr += len;
        sum += len;
	while (1)
	{
	    if (!object->char2int || (len = object->char2int(curr, NULL)) < 1)
		len = 1;
	    if (*curr == '\0' || (len == 1 && (isupper(*curr) != 0) != upper))
		break;
	    curr += len;
            sum += len;
	}
	/* 文節を登録する */
	if (start && start < curr)
	{
	    *pp = wordlist_open_len(start, sum);
	    pp = &(*pp)->next;
	}
	if (*curr == '\0')
	    break;
    }
    return querylist;
}

/*
 * 1つの単語をmigemo変換。引数のチェックは行なわない。
 */
    static int
query_a_word(migemo* object, unsigned char* query)
{
    unsigned char* zen;
    unsigned char* han;
    unsigned char* lower;
    int len = my_strlen(query);

    /* query自信はもちろん候補に加える */
    object->addword(object, query);
    /* queryそのものでの辞書引き */
    lower = malloc(len + 1);
    if (!lower)
	add_mnode_query(object, query);
    else
    {
	int i = 0, step;

	// MBを考慮した大文字→小文字変換
	while (i <= len)
	{
	    if (!object->char2int
		    || (step = object->char2int(&query[i], NULL)) < 1)
		step = 1;
	    if (step == 1 && isupper(query[i]))
		lower[i] = tolower(query[i]);
	    else
		memcpy(&lower[i], &query[i], step);
	    i += step;
	}
	add_mnode_query(object, lower);
	free(lower);
    }

    /* queryを全角にして候補に加える */
    zen = romaji_convert2(object->han2zen, query, NULL, 0);
    if (zen != NULL)
    {
	object->addword(object, zen);
	romaji_release(object->han2zen, zen);
    }

    /* queryを半角にして候補に加える */
    han = romaji_convert2(object->zen2han, query, NULL, 0);
    if (han != NULL)
    {
	object->addword(object, han);
	romaji_release(object->zen2han, han);
    }

    /* 平仮名、カタカナ、及びそれによる辞書引き追加 */
    if (add_roma(object, query))
	add_dubious_roma(object, object->rx, query);

    return 1;
}

    static int
addword_rxgen(migemo* object, unsigned char* word)
{
    /* 正規表現生成エンジンに追加された単語を表示する */
    /*printf("addword_rxgen: %s\n", word);*/
    return rxgen_add(object->rx, word);
}

/**
 * queryで与えられた文字列(ローマ字)を日本語検索のための正規表現へ変換する。
 * 戻り値は変換された結果の文字列(正規表現)で、使用後は#migemo_release()関数
 * へ渡すことで解放しなければならない。
 * @param object Migemoオブジェクト
 * @param query 問い合わせ文字列
 * @returns 正規表現文字列。#migemo_release() で解放する必要有り。
 */
    EXPORTS unsigned char* MIGEMO_CALLTYPE
migemo_query(migemo* object, const unsigned char* query)
{
    unsigned char *retval = NULL;
    wordlist_p querylist = NULL;
    wordbuf_p outbuf = NULL;

    if (object && object->rx && query)
    {
	wordlist_p p;

	querylist = parse_query(object, query);
	if (querylist == NULL)
	    goto MIGEMO_QUERY_END; /* 空queryのためエラー */
	outbuf = wordbuf_open();
	if (outbuf == NULL)
	    goto MIGEMO_QUERY_END; /* 出力用のメモリ領域不足のためエラー */

	/* 単語群をrxgenオブジェクトに入力し正規表現を得る */
	object->addword = (MIGEMO_PROC_ADDWORD)addword_rxgen;
	rxgen_reset(object->rx);
	for (p = querylist; p; p = p->next)
	{
	    unsigned char* answer;

	    /*printf("query=%s\n", p->ptr);*/
	    query_a_word(object, p->ptr);
	    /* 検索パターン(正規表現)生成 */
	    answer = rxgen_generate(object->rx);
	    rxgen_reset(object->rx);
	    wordbuf_cat(outbuf, answer);
	    rxgen_release(object->rx, answer);
	}
    }

MIGEMO_QUERY_END:
    if (outbuf)
    {
	retval = outbuf->buf;
	outbuf->buf = NULL;
	wordbuf_close(outbuf);
    }
    if (querylist)
	wordlist_close(querylist);

    return retval;
}

/**
 * 使い終わったmigemo_query()関数で得られた正規表現を解放する。
 * @param p Migemoオブジェクト
 * @param string 正規表現文字列
 */
    EXPORTS void MIGEMO_CALLTYPE
migemo_release(migemo* p, unsigned char* string)
{
    free(string);
}

/**
 * Migemoオブジェクトが生成する正規表現に使用するメタ文字(演算子)を指定す
 * る。indexでどのメタ文字かを指定し、opで置き換える。indexには以下の値が指
 * 定可能である:
 *
 *  <dl>
 *  <dt>MIGEMO_OPINDEX_OR</dt>
 *	<dd>論理和。デフォルトは "|" 。vimで利用する際は "\|" 。</dd>
 *  <dt>MIGEMO_OPINDEX_NEST_IN</dt>
 *	<dd>グルーピングに用いる開き括弧。デフォルトは "(" 。vimではレジスタ
 *	\\1～\\9に記憶させないようにするために "\%(" を用いる。Perlでも同様の
 *	ことを目論むならば "(?:" が使用可能。</dd>
 *  <dt>MIGEMO_OPINDEX_NEST_OUT</dt>
 *	<dd>グルーピングの終了を表す閉じ括弧。デフォルトでは ")" 。vimでは
 *	"\)" 。</dd>
 *  <dt>MIGEMO_OPINDEX_SELECT_IN</dt>
 *	<dd>選択の開始を表す開き角括弧。デフォルトでは "[" 。</dd>
 *  <dt>MIGEMO_OPINDEX_SELECT_OUT</dt>
 *	<dd>選択の終了を表す閉じ角括弧。デフォルトでは "]" 。</dd>
 *  <dt>MIGEMO_OPINDEX_NEWLINE</dt>
 *	<dd>各文字の間に挿入される「0個以上の空白もしくは改行にマッチする」
 *	パターン。デフォルトでは "" であり設定されない。vimでは "\_s*" を指
 *	定する。</dd>
 *  </dl>
 *
 * デフォルトのメタ文字は特に断りがない限りPerlのそれと同じ意味である。設定
 * に成功すると戻り値は1(0以外)となり、失敗すると0になる。
 * @param object Migemoオブジェクト
 * @param index メタ文字識別子
 * @param op メタ文字文字列
 * @returns 成功時0以外、失敗時0。
 */
    EXPORTS int MIGEMO_CALLTYPE
migemo_set_operator(migemo* object, int index, const unsigned char* op)
{
    if (object)
    {
	int retval = rxgen_set_operator(object->rx, index, op);
	return retval ? 0 : 1;
    }
    else
	return 0;
}

/**
 * Migemoオブジェクトが生成する正規表現に使用しているメタ文字(演算子)を取得
 * する。indexについてはmigemo_set_operator()関数を参照。戻り値にはindexの指
 * 定が正しければメタ文字を格納した文字列へのポインタが、不正であればNULLが
 * 返る。
 * @param object Migemoオブジェクト
 * @param index メタ文字識別子
 * @returns 現在のメタ文字文字列
 */
    EXPORTS const unsigned char* MIGEMO_CALLTYPE
migemo_get_operator(migemo* object, int index)
{
    return object ? rxgen_get_operator(object->rx, index) : NULL;
}

/**
 * Migemoオブジェクトにコード変換用のプロシージャを設定する。プロシージャに
 * ついての詳細は「型リファレンス」セクションのMIGEMO_PROC_CHAR2INTを参照。
 * @param object Migemoオブジェクト
 * @param proc コード変換用プロシージャ
 */
    EXPORTS void MIGEMO_CALLTYPE
migemo_setproc_char2int(migemo* object, MIGEMO_PROC_CHAR2INT proc)
{
    if (object)
	rxgen_setproc_char2int(object->rx, (RXGEN_PROC_CHAR2INT)proc);
}

/**
 * Migemoオブジェクトにコード変換用のプロシージャを設定する。プロシージャに
 * ついての詳細は「型リファレンス」セクションのMIGEMO_PROC_INT2CHARを参照。
 * @param object Migemoオブジェクト
 * @param proc コード変換用プロシージャ
 */
    EXPORTS void MIGEMO_CALLTYPE
migemo_setproc_int2char(migemo* object, MIGEMO_PROC_INT2CHAR proc)
{
    if (object)
	rxgen_setproc_int2char(object->rx, (RXGEN_PROC_INT2CHAR)proc);
}

/**
 * Migemoオブジェクトにmigemo_dictが読み込めているかをチェックする。有効な
 * migemo_dictを読み込めて内部に変換テーブルが構築できていれば0以外(TRUE)
 * を、構築できていないときには0(FALSE)を返す。
 * @param obj Migemoオブジェクト
 * @returns 成功時0以外、失敗時0。
 */
    EXPORTS int MIGEMO_CALLTYPE
migemo_is_enable(migemo* obj)
{
    return obj ? obj->enable : 0;
}

#if 1
/*
 * 主にデバッグ用の隠し関数
 */
    EXPORTS void MIGEMO_CALLTYPE
migemo_print(migemo* object)
{
    if (object)
	mnode_print(object->mtree, NULL);
}
#endif
