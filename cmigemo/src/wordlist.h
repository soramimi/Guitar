/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * wordlist.h -
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 * Last Change: 04-May-2004.
 */

#ifndef WORDLIST_H
#define WORDLIST_H

typedef struct _wordlist_t wordlist_t, *wordlist_p;
struct _wordlist_t
{
    unsigned char* ptr;
    wordlist_p next;
};

extern int n_wordlist_open;
extern int n_wordlist_close;
extern int n_wordlist_total;

#ifdef __cplusplus
extern "C" {
#endif

wordlist_p wordlist_open(const unsigned char* ptr);
wordlist_p wordlist_open_len(const unsigned char* ptr, int len);
void wordlist_close(wordlist_p p);

#ifdef __cplusplus
}
#endif

#endif /* WORDLIST_H */
