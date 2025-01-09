/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * charset.h -
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 * Last Change: 18-Jun-2004.
 */

#ifndef CHARSET_H
# define CHARSET_H

enum {
    CHARSET_NONE = 0,
    CHARSET_CP932 = 1,
    CHARSET_EUCJP = 2,
    CHARSET_UTF8 = 3,
};

typedef int (*charset_proc_char2int)(const unsigned char*, unsigned int*);
typedef int (*charset_proc_int2char)(unsigned int, unsigned char*);
#define CHARSET_PROC_CHAR2INT charset_proc_char2int
#define CHARSET_PROC_INT2CHAR charset_proc_int2char

#ifdef __cplusplus
extern "C" {
#endif

int cp932_char2int(const unsigned char* in, unsigned int* out);
int cp932_int2char(unsigned int in, unsigned char* out);
int eucjp_char2int(const unsigned char* in, unsigned int* out);
int eucjp_int2char(unsigned int in, unsigned char* out);
int utf8_char2int(const unsigned char* in, unsigned int* out);
int utf8_int2char(unsigned int in, unsigned char* out);

int charset_detect_file(const char* path);
int charset_detect_buf(const unsigned char* buf, int len);
void charset_getproc(int charset, CHARSET_PROC_CHAR2INT* char2int,
	CHARSET_PROC_INT2CHAR* int2char);

#ifdef __cplusplus
}
#endif

#endif /* CHARSET_H */
