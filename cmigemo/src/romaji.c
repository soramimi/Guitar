/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * romaji.c - ローマ字変換
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "wordbuf.h"
#include "charset.h"
#include "romaji.h"

#if defined(_MSC_VER) || defined(__GNUC__)
# define INLINE __inline
#else
# define INLINE 
#endif

#ifdef _DEBUG
# define VERBOSE(o,l,b)	if ((o)->verbose >= (l)) { b }
#else
# define VERBOSE(o,l,b)
#endif

#if defined(_MSC_VER)
# define STRDUP _strdup
#else
# define STRDUP strdup
#endif

#define ROMAJI_FIXKEY_N 'n'
#define ROMAJI_FIXKEY_XN "xn"
#define ROMAJI_FIXKEY_XTU "xtu"
#define ROMAJI_FIXKEY_NONXTU "aiueon"

/*
 * romanode interfaces
 */

typedef struct _romanode romanode;
struct _romanode
{
    unsigned char key;
    unsigned char* value;
    romanode* next;
    romanode* child;
};

int n_romanode_new = 0;
int n_romanode_delete = 0;

    INLINE static romanode*
romanode_new()
{
    ++n_romanode_new;
    return (romanode*)calloc(1, sizeof(romanode));
}

    static void
romanode_delete(romanode* node)
{
    while (node)
    {
	romanode* child = node->child;
	if (node->next)
	    romanode_delete(node->next);
	free(node->value);
	free(node);
	node = child;
	++n_romanode_delete;
    }
}

    static romanode**
romanode_dig(romanode** ref_node, const unsigned char* key)
{
    if (!ref_node || !key || key[0] == '\0')
	return NULL;

    while (1)
    {
	if (!*ref_node)
	{
	    if (!(*ref_node = romanode_new()))
		return NULL;
	    (*ref_node)->key = *key;
	}

	if ((*ref_node)->key == *key)
	{
	    (*ref_node)->value = NULL;
	    if (!*++key)
		break;
	    ref_node = &(*ref_node)->child;
	}
	else
	    ref_node = &(*ref_node)->next;
    }

    if ((*ref_node)->child)
    {
	romanode_delete((*ref_node)->child);
	(*ref_node)->child = 0;
    }
    return ref_node;
}

/**
 * キーに対応したromanodeを検索して返す。
 * @return romanodeが見つからなかった場合NULL
 * @param node ルートノード
 * @param key 検索キー
 * @param skip 進めるべきkeyのバイト数を受け取るポインタ
 */
    static romanode*
romanode_query(romanode* node, const unsigned char* key, int* skip,
	ROMAJI_PROC_CHAR2INT char2int)
{
    int nskip = 0;
    const unsigned char* key_start = key;

    //printf("romanode_query: key=%s skip=%p char2int=%p\n", key, skip, char2int);
    if (node && key && *key)
    {
	while (1)
	{
	    if (*key != node->key)
		node = node->next;
	    else
	    {
		++nskip;
		if (node->value)
		{
		    //printf("  HERE 1\n");
		    break;
		}
		if (!*++key)
		{
		    nskip = 0;
		    //printf("  HERE 2\n");
		    break;
		}
		node = node->child;
	    }
	    /* 次に走査するノードが空の場合、キーを進めてNULLを返す */
	    if (!node)
	    {
		/* 1バイトではなく1文字進める */
		if (!char2int || (nskip = (*char2int)(key_start, NULL)) < 1)
		    nskip = 1;
		//printf("  HERE 3: nskip=%d\n", nskip);
		break;
	    }
	}
    }

    if (skip)
	*skip = nskip;
    return node;
}

#if 0 /* 未使用のため */
    static void
romanode_print_stub(romanode* node, unsigned char* p)
{
    static unsigned char buf[256];

    if (!p)
	p = &buf[0];
    p[0] = node->key;
    p[1] = '\0';
    if (node->value)
	printf("%s=%s\n", buf, node->value);
    if (node->child)
	romanode_print_stub(node->child, p + 1);
    if (node->next)
	romanode_print_stub(node->next, p);
}

    static void
romanode_print(romanode* node)
{
    if (!node)
	return;
    romanode_print_stub(node, NULL);
}
#endif

/*
 * romaji interfaces
 */

struct _romaji
{
    int verbose;
    romanode* node;
    unsigned char* fixvalue_xn;
    unsigned char* fixvalue_xtu;
    ROMAJI_PROC_CHAR2INT char2int;
};

    static unsigned char*
strdup_lower(const unsigned char* string)
{
    unsigned char *out = STRDUP(string), *tmp;

    if (out)
	for (tmp = out; *tmp; ++tmp)
	    *tmp = tolower(*tmp);
    return out;
}

    romaji*
romaji_open()
{
    return (romaji*)calloc(1, sizeof(romaji));
}

    void
romaji_close(romaji* object)
{
    if (object)
    {
	if (object->node)
	    romanode_delete(object->node);
	free(object->fixvalue_xn);
	free(object->fixvalue_xtu);
	free(object);
    }
}

    int
romaji_add_table(romaji* object, const unsigned char* key,
	const unsigned char* value)
{
    int value_length;
    romanode **ref_node;

    if (!object || !key || !value)
	return 1; /* Unexpected error */

    value_length = strlen(value);
    if (value_length == 0)
	return 2; /* Too short value string */

    if (!(ref_node = romanode_dig(&object->node, key)))
    {
	return 4; /* Memory exhausted */
    }
    VERBOSE(object, 10,
	    printf("romaji_add_table(\"%s\", \"%s\")\n", key, value););
    (*ref_node)->value = STRDUP(value);

    /* 「ん」と「っ」は保存しておく */
    if (object->fixvalue_xn == NULL && value_length > 0
	    && !strcmp(key, ROMAJI_FIXKEY_XN))
    {
	/*fprintf(stderr, "XN: key=%s, value=%s\n", key, value);*/
	object->fixvalue_xn = STRDUP(value);
    }
    if (object->fixvalue_xtu == NULL && value_length > 0
	    && !strcmp(key, ROMAJI_FIXKEY_XTU))
    {
	/*fprintf(stderr, "XTU: key=%s, value=%s\n", key, value);*/
	object->fixvalue_xtu = STRDUP(value);
    }

    return 0;
}

    int
romaji_load_stub(romaji* object, FILE* fp)
{
    int mode, ch;
    wordbuf_p buf_key;
    wordbuf_p buf_value;
    
    buf_key = wordbuf_open();
    buf_value = wordbuf_open();
    if (!buf_key || !buf_value)
    {
	wordbuf_close(buf_key);
	wordbuf_close(buf_value);
	return -1;
    }

    mode = 0;
    do
    {
	ch = fgetc(fp);
	switch (mode)
	{
	    case 0:
		/* key待ちモード */
		if (ch == '#')
		{
		    /* 1文字先読みして空白ならばkeyとして扱う */
		    ch = fgetc(fp);
		    if (ch != '#')
		    {
			ungetc(ch, fp);
			mode = 1; /* 行末まで読み飛ばしモード へ移行 */
			break;
		    }
		}
		if (ch != EOF && !isspace(ch))
		{
		    wordbuf_reset(buf_key);
		    wordbuf_add(buf_key, (unsigned char)ch);
		    mode = 2; /* key読み込みモード へ移行 */
		}
		break;

	    case 1:
		/* 行末まで読み飛ばしモード */
		if (ch == '\n')
		    mode = 0; /* key待ちモード へ移行 */
		break;

	    case 2:
		/* key読み込みモード */
		if (!isspace(ch))
		    wordbuf_add(buf_key, (unsigned char)ch);
		else
		    mode = 3; /* value待ちモード へ移行 */
		break;

	    case 3:
		/* value待ちモード */
		if (ch != EOF && !isspace(ch))
		{
		    wordbuf_reset(buf_value);
		    wordbuf_add(buf_value, (unsigned char)ch);
		    mode = 4; /* value読み込みモード へ移行 */
		}
		break;

	    case 4:
		/* value読み込みモード */
		if (ch != EOF && !isspace(ch))
		    wordbuf_add(buf_value, (unsigned char)ch);
		else
		{
		    unsigned char *key = WORDBUF_GET(buf_key);
		    unsigned char *value = WORDBUF_GET(buf_value);
		    romaji_add_table(object, key, value);
		    mode = 0;
		}
		break;
	}
    }
    while (ch != EOF);

    wordbuf_close(buf_key);
    wordbuf_close(buf_value);
    return 0;
}

/**
 * ローマ字辞書を読み込む。
 * @param object ローマ字オブジェクト
 * @param filename 辞書ファイル名
 * @return 成功した場合0、失敗した場合は非0を返す。
 */
    int
romaji_load(romaji* object, const unsigned char* filename)
{
    FILE *fp;
    int charset;
    if (!object || !filename)
	return -1;
#if 1
    charset = charset_detect_file(filename);
    charset_getproc(charset, (CHARSET_PROC_CHAR2INT*)&object->char2int, NULL);
#endif
    if ((fp = fopen(filename, "rt")) != NULL)
    {
	int result = result = romaji_load_stub(object, fp);
	fclose(fp);
	return result;
    }
    else
	return -1;
}

    unsigned char*
romaji_convert2(romaji* object, const unsigned char* string,
	unsigned char** ppstop, int ignorecase)
{
    /* Argument "ppstop" receive conversion stoped position. */
    wordbuf_p buf = NULL;
    unsigned char *lower = NULL;
    unsigned char *answer = NULL;
    const unsigned char *input = string;
    int stop = -1;

    if (ignorecase)
    {
	lower = strdup_lower(string);
	input = lower;
    }

    if (object && string && input && (buf = wordbuf_open()))
    {
	int i;

	for (i = 0; string[i]; )
	{
	    romanode *node;
	    int skip;

	    /* 「っ」の判定 */
	    if (object->fixvalue_xtu && input[i] == input[i + 1]
		    && !strchr(ROMAJI_FIXKEY_NONXTU, input[i]))
	    {
		++i;
		wordbuf_cat(buf, object->fixvalue_xtu);
		continue;
	    }

	    node = romanode_query(object->node, &input[i], &skip, object->char2int);
	    VERBOSE(object, 1, printf("key=%s value=%s skip=%d\n", &input[i], node ? (char*)node->value : "null" , skip);)
	    if (skip == 0)
	    {
		if (string[i])
		{
		    stop = WORDBUF_LEN(buf);
		    wordbuf_cat(buf, &string[i]);
		}
		break;
	    }
	    else if (!node)
	    {
		/* 「n(子音)」を「ん(子音)」に変換 */
		if (skip == 1 && input[i] == ROMAJI_FIXKEY_N
			&& object->fixvalue_xn)
		{
		    ++i;
		    wordbuf_cat(buf, object->fixvalue_xn);
		}
		else
		    while (skip--)
			wordbuf_add(buf, string[i++]);
	    }
	    else
	    {
		i += skip;
		wordbuf_cat(buf, node->value);
	    }
	}
	answer = STRDUP(WORDBUF_GET(buf));
    }
    if (ppstop)
	*ppstop = (stop >= 0) ? answer + stop : NULL;

    if (lower)
	free(lower);
    if (buf)
	wordbuf_close(buf);
    return answer;
}

    unsigned char*
romaji_convert(romaji* object, const unsigned char* string,
	unsigned char** ppstop)
{
    return romaji_convert2(object, string, ppstop, 1);
}

    void
romaji_release(romaji* object, unsigned char* string)
{
    free(string);
}

    void
romaji_setproc_char2int(romaji* object, ROMAJI_PROC_CHAR2INT proc)
{
    if (object)
	object->char2int = proc;
}

    void
romaji_set_verbose(romaji* object, int level)
{
    if (object)
	object->verbose = level;
}
