/* vi:set ts=8 sts=4 sw=4 tw=0: */
/*
 * filename.h - Operate filename.
 *
 * Last change: 15-May-2002.
 * Written by:  Muraoka Taro  <koron@tka.att.ne.jp>
 */

#ifndef FILENAME_H
#define FILENAME_H

#ifdef __cplusplus
extern "C" {
#endif

int filename_base(char *base, const char *path);
int filename_directory(char *dir, const char *path);
int filename_extension(char *ext, const char *path);
int filename_filename(char *name, const char *path);
int filename_generate(char *filepath, const char *dir, const char *base, const char *ext);

#ifdef __cplusplus
}
#endif

#endif /* FILENAME_H */
