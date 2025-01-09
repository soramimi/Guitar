/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * rxgen.h - regular expression generator
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 * Last Change: 04-May-2004.
 */

#ifndef RXGEN_H
#define RXGEN_H

typedef struct _rxgen rxgen;
typedef int (*rxgen_proc_char2int)(const unsigned char*, unsigned int*);
typedef int (*rxgen_proc_int2char)(unsigned int, unsigned char*);
#define RXGEN_PROC_CHAR2INT rxgen_proc_char2int
#define RXGEN_PROC_INT2CHAR rxgen_proc_int2char

/* for rxgen_set_operator */
#define RXGEN_OPINDEX_OR		0
#define RXGEN_OPINDEX_NEST_IN		1
#define RXGEN_OPINDEX_NEST_OUT		2
#define RXGEN_OPINDEX_SELECT_IN		3
#define RXGEN_OPINDEX_SELECT_OUT	4
#define RXGEN_OPINDEX_NEWLINE		5

extern int n_rnode_new;
extern int n_rnode_delete;

#ifdef __cplusplus
extern "C" {
#endif

rxgen* rxgen_open();
void rxgen_close(rxgen* object);
int rxgen_add(rxgen* object, const unsigned char* word);
unsigned char* rxgen_generate(rxgen* object);
void rxgen_release(rxgen* object, unsigned char* string);
void rxgen_reset(rxgen* object);

void rxgen_setproc_char2int(rxgen* object, RXGEN_PROC_CHAR2INT proc);
void rxgen_setproc_int2char(rxgen* object, RXGEN_PROC_INT2CHAR proc);
int rxgen_set_operator(rxgen* object, int index, const unsigned char* op);
const unsigned char* rxgen_get_operator(rxgen* object, int index);

#ifdef __cplusplus
}
#endif

#endif /* RXGEN_H */
