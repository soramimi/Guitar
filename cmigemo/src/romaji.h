/* vi:set ts=8 sts=4 sw=4 tw=0: */
/*
 * romaji.h - ローマ字変換
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 * Last Change: 19-Jun-2004.
 */

#ifndef ROMAJI_H
#define ROMAJI_H

typedef struct _romaji romaji;
typedef int (*romaji_proc_char2int)(const unsigned char*, unsigned int*);
#define ROMAJI_PROC_CHAR2INT romaji_proc_char2int

#ifdef __cplusplus
extern "C" {
#endif

romaji* romaji_open();
void romaji_close(romaji* object);
int romaji_add_table(romaji* object, const unsigned char* key,
	const unsigned char* value);
int romaji_load(romaji* object, const unsigned char* filename);
unsigned char* romaji_convert(romaji* object, const unsigned char* string,
	unsigned char** ppstop);
unsigned char* romaji_convert2(romaji* object, const unsigned char* string,
	unsigned char** ppstop, int ignorecase);
void romaji_release(romaji* object, unsigned char* string);

void romaji_setproc_char2int(romaji* object, ROMAJI_PROC_CHAR2INT proc);
void romaji_set_verbose(romaji* object, int level);

#ifdef __cplusplus
}
#endif

#endif /* ROMAJI_H */
