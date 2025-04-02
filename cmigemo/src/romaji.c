/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * romaji.c - Romaji conversion
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 */

#include "romaji.h"
#include "charset.h"
#include "wordbuf.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER) || defined(__GNUC__)
#define INLINE __inline
#else
#define INLINE
#endif

#ifdef _DEBUG
#define VERBOSE(o, l, b)       \
	if ((o)->verbose >= (l)) { \
		b                      \
	}
#else
#define VERBOSE(o, l, b)
#endif

#if defined(_MSC_VER)
#define STRDUP _strdup
#else
#define STRDUP strdup
#endif

#define ROMAJI_FIXKEY_N 'n'
#define ROMAJI_FIXKEY_XN "xn"
#define ROMAJI_FIXKEY_XTU "xtu"
#define ROMAJI_FIXKEY_NONXTU "aiueon"

/*
 * romanode interfaces
 */

typedef struct _romanode romanode;
struct _romanode {
	unsigned char key;
	unsigned char *value;
	romanode *next;
	romanode *child;
};

int n_romanode_new = 0;
int n_romanode_delete = 0;

INLINE static romanode *
romanode_new()
{
	++n_romanode_new;
	return (romanode *)calloc(1, sizeof(romanode));
}

static void
romanode_delete(romanode *node)
{
	while (node) {
		romanode *child = node->child;
		if (node->next)
			romanode_delete(node->next);
		free(node->value);
		free(node);
		node = child;
		++n_romanode_delete;
	}
}

static romanode **
romanode_dig(romanode **ref_node, const unsigned char *key)
{
	if (!ref_node || !key || key[0] == '\0')
		return NULL;

	while (1) {
		if (!*ref_node) {
			if (!(*ref_node = romanode_new()))
				return NULL;
			(*ref_node)->key = *key;
		}

		if ((*ref_node)->key == *key) {
			(*ref_node)->value = NULL;
			if (!*++key)
				break;
			ref_node = &(*ref_node)->child;
		} else
			ref_node = &(*ref_node)->next;
	}

	if ((*ref_node)->child) {
		romanode_delete((*ref_node)->child);
		(*ref_node)->child = 0;
	}
	return ref_node;
}

/**
 * Search for and return the romanode corresponding to the key.
 * @return NULL if romanode is not found
 * @param node Root node
 * @param key Search key
 * @param skip Pointer to receive the number of bytes to advance the key
 */
static romanode *
romanode_query(romanode *node, const unsigned char *key, int *skip,
	ROMAJI_PROC_CHAR2INT char2int)
{
	int nskip = 0;
	const unsigned char *key_start = key;

	// printf("romanode_query: key=%s skip=%p char2int=%p\n", key, skip, char2int);
	if (node && key && *key) {
		while (1) {
			if (*key != node->key)
				node = node->next;
			else {
				++nskip;
				if (node->value) {
					// printf("  HERE 1\n");
					break;
				}
				if (!*++key) {
					nskip = 0;
					// printf("  HERE 2\n");
					break;
				}
				node = node->child;
			}
			/* When the next node to scan is empty, advance the key and return NULL */
			if (!node) {
				/* Advance by one character, not one byte */
				if (!char2int || (nskip = (*char2int)(key_start, NULL)) < 1)
					nskip = 1;
				// printf("  HERE 3: nskip=%d\n", nskip);
				break;
			}
		}
	}

	if (skip)
		*skip = nskip;
	return node;
}

#if 0 /* Not used */
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

struct _romaji {
	int verbose;
	romanode *node;
	unsigned char *fixvalue_xn;
	unsigned char *fixvalue_xtu;
	ROMAJI_PROC_CHAR2INT char2int;
};

static unsigned char *
strdup_lower(const unsigned char *string)
{
	unsigned char *out = STRDUP(string), *tmp;

	if (out)
		for (tmp = out; *tmp; ++tmp)
			*tmp = tolower(*tmp);
	return out;
}

romaji *
romaji_open()
{
	return (romaji *)calloc(1, sizeof(romaji));
}

void romaji_close(romaji *object)
{
	if (object) {
		if (object->node)
			romanode_delete(object->node);
		free(object->fixvalue_xn);
		free(object->fixvalue_xtu);
		free(object);
	}
}

int romaji_add_table(romaji *object, const unsigned char *key,
	const unsigned char *value)
{
	int value_length;
	romanode **ref_node;

	if (!object || !key || !value)
		return 1; /* Unexpected error */

	value_length = strlen(value);
	if (value_length == 0)
		return 2; /* Too short value string */

	if (!(ref_node = romanode_dig(&object->node, key))) {
		return 4; /* Memory exhausted */
	}
	VERBOSE(object, 10,
			printf("romaji_add_table(\"%s\", \"%s\")\n", key, value););
	(*ref_node)->value = STRDUP(value);

	/* Store 'ん' (n) and 'っ' (small tsu) */
	if (object->fixvalue_xn == NULL && value_length > 0
		&& !strcmp(key, ROMAJI_FIXKEY_XN)) {
		/*fprintf(stderr, "XN: key=%s, value=%s\n", key, value);*/
		object->fixvalue_xn = STRDUP(value);
	}
	if (object->fixvalue_xtu == NULL && value_length > 0
		&& !strcmp(key, ROMAJI_FIXKEY_XTU)) {
		/*fprintf(stderr, "XTU: key=%s, value=%s\n", key, value);*/
		object->fixvalue_xtu = STRDUP(value);
	}

	return 0;
}

int romaji_load_stub(romaji *object, FILE *fp)
{
	int mode, ch;
	wordbuf_p buf_key;
	wordbuf_p buf_value;

	buf_key = wordbuf_open();
	buf_value = wordbuf_open();
	if (!buf_key || !buf_value) {
		wordbuf_close(buf_key);
		wordbuf_close(buf_value);
		return -1;
	}

	mode = 0;
	do {
		ch = fgetc(fp);
		switch (mode) {
		case 0:
			/* Waiting for key mode */
			if (ch == '#') {
				/* Read one character ahead, treat as key if it's not a whitespace */
				ch = fgetc(fp);
				if (ch != '#') {
					ungetc(ch, fp);
					mode = 1; /* Switch to skip-to-end-of-line mode */
					break;
				}
			}
			if (ch != EOF && !isspace(ch)) {
				wordbuf_reset(buf_key);
				wordbuf_add(buf_key, (unsigned char)ch);
				mode = 2; /* Switch to key reading mode */
			}
			break;

		case 1:
			/* Skip-to-end-of-line mode */
			if (ch == '\n')
				mode = 0; /* Switch to waiting for key mode */
			break;

		case 2:
			/* Key reading mode */
			if (!isspace(ch))
				wordbuf_add(buf_key, (unsigned char)ch);
			else
				mode = 3; /* Switch to waiting for value mode */
			break;

		case 3:
			/* Waiting for value mode */
			if (ch != EOF && !isspace(ch)) {
				wordbuf_reset(buf_value);
				wordbuf_add(buf_value, (unsigned char)ch);
				mode = 4; /* Switch to value reading mode */
			}
			break;

		case 4:
			/* Value reading mode */
			if (ch != EOF && !isspace(ch))
				wordbuf_add(buf_value, (unsigned char)ch);
			else {
				unsigned char *key = WORDBUF_GET(buf_key);
				unsigned char *value = WORDBUF_GET(buf_value);
				romaji_add_table(object, key, value);
				mode = 0;
			}
			break;
		}
	} while (ch != EOF);

	wordbuf_close(buf_key);
	wordbuf_close(buf_value);
	return 0;
}

/**
 * Load romaji dictionary.
 * @param object Romaji object
 * @param filename Dictionary filename
 * @return Returns 0 on success, non-zero on failure.
 */
int romaji_load(romaji *object, const unsigned char *filename)
{
	FILE *fp;
	int charset;
	if (!object || !filename)
		return -1;
#if 1
	charset = charset_detect_file(filename);
	charset_getproc(charset, (CHARSET_PROC_CHAR2INT *)&object->char2int, NULL);
#endif
	if ((fp = fopen(filename, "rt")) != NULL) {
		int result = result = romaji_load_stub(object, fp);
		fclose(fp);
		return result;
	} else
		return -1;
}

unsigned char *
romaji_convert2(romaji *object, const unsigned char *string,
	unsigned char **ppstop, int ignorecase)
{
	/* Argument "ppstop" receive conversion stoped position. */
	wordbuf_p buf = NULL;
	unsigned char *lower = NULL;
	unsigned char *answer = NULL;
	const unsigned char *input = string;
	int stop = -1;

	if (ignorecase) {
		lower = strdup_lower(string);
		input = lower;
	}

	if (object && string && input && (buf = wordbuf_open())) {
		int i;

		for (i = 0; string[i];) {
			romanode *node;
			int skip;

			/* Detection of 'っ' (small tsu) */
			if (object->fixvalue_xtu && input[i] == input[i + 1]
				&& !strchr(ROMAJI_FIXKEY_NONXTU, input[i])) {
				++i;
				wordbuf_cat(buf, object->fixvalue_xtu);
				continue;
			}

			node = romanode_query(object->node, &input[i], &skip, object->char2int);
			VERBOSE(object, 1, printf("key=%s value=%s skip=%d\n", &input[i], node ? (char *)node->value : "null", skip);)
			if (skip == 0) {
				if (string[i]) {
					stop = WORDBUF_LEN(buf);
					wordbuf_cat(buf, &string[i]);
				}
				break;
			} else if (!node) {
				/* Convert 'n(consonant)' to 'ん(consonant)' */
				if (skip == 1 && input[i] == ROMAJI_FIXKEY_N
					&& object->fixvalue_xn) {
					++i;
					wordbuf_cat(buf, object->fixvalue_xn);
				} else
					while (skip--)
						wordbuf_add(buf, string[i++]);
			} else {
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

unsigned char *
romaji_convert(romaji *object, const unsigned char *string,
	unsigned char **ppstop)
{
	return romaji_convert2(object, string, ppstop, 1);
}

void romaji_release(romaji *object, unsigned char *string)
{
	free(string);
}

void romaji_setproc_char2int(romaji *object, ROMAJI_PROC_CHAR2INT proc)
{
	if (object)
		object->char2int = proc;
}

void romaji_set_verbose(romaji *object, int level)
{
	if (object)
		object->verbose = level;
}
