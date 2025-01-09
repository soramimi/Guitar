/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * mnode.c - mnode interfaces.
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 * Last Change: 04-May-2004.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "dbg.h"
#include "wordlist.h"
#include "wordbuf.h"
#include "mnode.h"

#define MTREE_MNODE_N 1024
struct _mtree_t
{
    mtree_p	active;
    int		used;
    mnode	nodes[MTREE_MNODE_N];
    mtree_p	next;
};

#define MNODE_BUFSIZE 16384

#if defined(_MSC_VER) || defined(__GNUC__)
# define INLINE __inline
#else
# define INLINE 
#endif

int n_mnode_new = 0;
int n_mnode_delete = 0;

    INLINE static mnode*
mnode_new(mtree_p mtree)
{
    mtree_p active = mtree->active;

    if (active->used >= MTREE_MNODE_N)
    {
	active->next = (mtree_p)calloc(1, sizeof(*active->next));
	/* TODO: エラー処理 */
	mtree->active = active->next;
	active = active->next;
    }
    ++n_mnode_new;
    return &active->nodes[active->used++];
}

    static void
mnode_delete(mnode* p)
{
    while (p)
    {
	mnode* child = p->child;

	if (p->list)
	    wordlist_close(p->list);
	if (p->next)
	    mnode_delete(p->next);
	/*free(p);*/
	p = child;
	++n_mnode_delete;
    }
}


    void
mnode_print_stub(mnode* vp, unsigned char* p)
{
    static unsigned char buf [256];

    if (!vp)
	return;
    if (!p)
	p = &buf[0];
    p[0] = MNODE_GET_CH(vp);
    p[1] = '\0';
    if (vp->list)
	printf("%s (list=%p)\n", buf, vp->list);
    if (vp->child)
	mnode_print_stub(vp->child, p + 1);
    if (vp->next)
	mnode_print_stub(vp->next, p);
}

    void
mnode_print(mtree_p mtree, unsigned char* p)
{
    if (mtree && mtree->used > 0)
	mnode_print_stub(&mtree->nodes[0], p);
}

    void
mnode_close(mtree_p mtree)
{
    if (mtree)
    {
	mtree_p next;

	if (mtree->used > 0)
	    mnode_delete(&mtree->nodes[0]);

	while (mtree)
	{
	    next = mtree->next;
	    free(mtree);
	    mtree = next;
	}
    }
}

    INLINE static mnode*
search_or_new_mnode(mtree_p mtree, wordbuf_p buf)
{
    /* ラベル単語が決定したら検索木に追加 */
    int ch;
    unsigned char *word;
    mnode **ppnext;
    mnode **res = NULL; /* To suppress warning for GCC */
    mnode *root;

    word = WORDBUF_GET(buf);
    root = mtree->used > 0 ? &mtree->nodes[0] : NULL;
    ppnext = &root;
    while ((ch = *word) != 0)
    {
	res = ppnext;
	if (! *res)
	{
	    *res = mnode_new(mtree);
	    MNODE_SET_CH(*res, ch);
	}
	else if (MNODE_GET_CH(*res) != ch)
	{
	    ppnext = &(*res)->next;
	    continue;
	}
	ppnext = &(*res)->child;
	++word;
    }

    _ASSERT(*res != NULL);
    return *res;
}

/*
 * 既存のノードにファイルからデータをまとめて追加する。
 */
    mtree_p
mnode_load(mtree_p mtree, FILE* fp)
{
    mnode *pp = NULL;
    int mode = 0;
    int ch;
    wordbuf_p buf;
    wordbuf_p prevlabel;
    wordlist_p *ppword = NULL; /* To suppress warning for GCC */
    /* 読み込みバッファ用変数 */
    unsigned char cache[MNODE_BUFSIZE];
    unsigned char *cache_ptr = cache;
    unsigned char *cache_tail = cache;

    buf = wordbuf_open();
    prevlabel = wordbuf_open();
    if (!fp || !buf || !prevlabel)
    {
	goto END_MNODE_LOAD;
    }

    /*
     * EOFの処理が曖昧。不正な形式のファイルが入った場合を考慮していない。各
     * モードからEOFの道を用意しないと正しくないが…面倒なのでやらない。デー
     * タファイルは絶対に間違っていないという前提を置く。
     */
    do
    {
	if (cache_ptr >= cache_tail)
	{
	    cache_ptr = cache;
	    cache_tail = cache + fread(cache, 1, MNODE_BUFSIZE, fp);
	    ch = (cache_tail <= cache && feof(fp)) ? EOF : *cache_ptr;
	}
	else
	    ch = *cache_ptr;
	++cache_ptr;

	/* 状態:modeのオートマトン */
	switch (mode)
	{
	    case 0: /* ラベル単語検索モード */
		/* 空白はラベル単語になりえません */
		if (isspace(ch) || ch == EOF)
		    continue;
		/* コメントラインチェック */
		else if (ch == ';')
		{
		    mode = 2; /* 行末まで食い潰すモード へ移行 */
		    continue;
		}
		else
		{
		    mode = 1; /* ラベル単語の読込モード へ移行*/
		    wordbuf_reset(buf);
		    wordbuf_add(buf, (unsigned char)ch);
		}
		break;

	    case 1: /* ラベル単語の読込モード */
		/* ラベルの終了を検出 */
		switch (ch)
		{
		    default:
			wordbuf_add(buf, (unsigned char)ch);
			break;
		    case '\t':
			pp = search_or_new_mnode(mtree, buf);
			wordbuf_reset(buf);
			mode = 3; /* 単語前空白読飛ばしモード へ移行 */
			break;
		}
		break;

	    case 2: /* 行末まで食い潰すモード */
		if (ch == '\n')
		{
		    wordbuf_reset(buf);
		    mode = 0; /* ラベル単語検索モード へ戻る */
		}
		break;

	    case 3: /* 単語前空白読み飛ばしモード */
		if (ch == '\n')
		{
		    wordbuf_reset(buf);
		    mode = 0; /* ラベル単語検索モード へ戻る */
		}
		else if (ch != '\t')
		{
		    /* 単語バッファリセット */
		    wordbuf_reset(buf);
		    wordbuf_add(buf, (unsigned char)ch);
		    /* 単語リストの最後を検索(同一ラベルが複数時) */
		    ppword = &pp->list;
		    while (*ppword)
			ppword = &(*ppword)->next;
		    mode = 4; /* 単語の読み込みモード へ移行 */
		}
		break;

	    case 4: /* 単語の読み込みモード */
		switch (ch)
		{
		    case '\t':
		    case '\n':
			/* 単語を記憶 */
			*ppword = wordlist_open_len(WORDBUF_GET(buf),
				WORDBUF_LEN(buf));
			wordbuf_reset(buf);

			if (ch == '\t')
			{
			    ppword = &(*ppword)->next;
			    mode = 3; /* 単語前空白読み飛ばしモード へ戻る */
			}
			else
			{
			    ppword = NULL;
			    mode = 0; /* ラベル単語検索モード へ戻る */
			}
			break;
		    default:
			wordbuf_add(buf, (unsigned char)ch);
			break;
		}
		break;
	}
    }
    while (ch != EOF);

END_MNODE_LOAD:
    wordbuf_close(buf);
    wordbuf_close(prevlabel);
    return mtree;
}

    mtree_p
mnode_open(FILE* fp)
{
    mtree_p mtree;

    mtree = (mtree_p)calloc(1, sizeof(*mtree));
    mtree->active = mtree;
    if (mtree && fp)
	mnode_load(mtree, fp);

    return mtree;
}

#if 0
    static int
mnode_size(mnode* p)
{
    return p ? mnode_size(p->child) + mnode_size(p->next) + 1 : 0;
}
#endif

    static mnode*
mnode_query_stub(mnode* node, const unsigned char* query)
{
    while (1)
    {
	if (*query == MNODE_GET_CH(node))
	    return (*++query == '\0') ? node :
		(node->child ? mnode_query_stub(node->child, query) : NULL);
	if (!(node = node->next))
	    break;
    }
    return NULL;
}

    mnode*
mnode_query(mtree_p mtree, const unsigned char* query)
{
    return (query && *query != '\0' && mtree)
	? mnode_query_stub(&mtree->nodes[0], query) : 0;
}

    static void
mnode_traverse_stub(mnode* node, MNODE_TRAVERSE_PROC proc, void* data)
{
    while (1)
    {
	if (node->child)
	    mnode_traverse_stub(node->child, proc, data);
	proc(node, data);
	if (!(node = node->next))
	    break;
    }
}

    void
mnode_traverse(mnode *node, MNODE_TRAVERSE_PROC proc, void* data)
{
    if (node && proc)
    {
	proc(node, data);
	if (node->child)
	    mnode_traverse_stub(node->child, proc, data);
    }
}
