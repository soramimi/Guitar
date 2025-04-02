/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * migemo.c -
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "charset.h"
#include "filename.h"
#include "migemo.h"
#include "mnode.h"
#include "romaji.h"
#include "rxgen.h"
#include "wordbuf.h"
#include "wordlist.h"

#define DICT_MIGEMO "migemo-dict"
#define DICT_ROMA2HIRA "roma2hira.dat"
#define DICT_HIRA2KATA "hira2kata.dat"
#define DICT_HAN2ZEN "han2zen.dat"
#define DICT_ZEN2HAN "zen2han.dat"
#define BUFLEN_DETECT_CHARSET 4096

#ifdef __BORLANDC__
#define EXPORTS __declspec(dllexport)
#else
#define EXPORTS
#endif

typedef int (*MIGEMO_PROC_ADDWORD)(void *data, unsigned char *word);

/* Migemo object */
struct _migemo {
	int enable;
	mtree_p mtree;
	int charset;
	romaji *roma2hira;
	romaji *hira2kata;
	romaji *han2zen;
	romaji *zen2han;
	rxgen *rx;
	MIGEMO_PROC_ADDWORD addword;
	CHARSET_PROC_CHAR2INT char2int;
};

static const unsigned char VOWEL_CHARS[] = "aiueo";

static int
my_strlen(const char *s)
{
	size_t len;

	len = strlen(s);
	return len <= INT_MAX ? (int)len : INT_MAX;
}

static mtree_p
load_mtree_dictionary(mtree_p mtree, const char *dict_file)
{
	FILE *fp;

	if ((fp = fopen(dict_file, "rt")) == NULL)
		return NULL; /* Can't find file */
	mtree = mnode_load(mtree, fp);
	fclose(fp);
	return mtree;
}

static mtree_p
load_mtree_dictionary2(migemo *obj, const char *dict_file)
{
	if (obj->charset == CHARSET_NONE) {
		/* Change functions for regex generation to match the dictionary character set */
		CHARSET_PROC_CHAR2INT char2int = NULL;
		CHARSET_PROC_INT2CHAR int2char = NULL;
		obj->charset = charset_detect_file(dict_file);
		charset_getproc(obj->charset, &char2int, &int2char);
		if (char2int) {
			migemo_setproc_char2int(obj, (MIGEMO_PROC_CHAR2INT)char2int);
			obj->char2int = char2int;
		}
		if (int2char)
			migemo_setproc_int2char(obj, (MIGEMO_PROC_INT2CHAR)int2char);
	}
	return load_mtree_dictionary(obj->mtree, dict_file);
}

static void
dircat(char *buf, const char *dir, const char *file)
{
	strcpy(buf, dir);
	strcat(buf, "/");
	strcat(buf, file);
}

/*
 * migemo interfaces
 */

/**
 * Add a dictionary or data file to the Migemo object.
 * dict_file specifies the file name to load. dict_id specifies the type of dictionary/data
 * to be loaded, which must be one of the following:
 *
 *  <dl>
 *  <dt>MIGEMO_DICTID_MIGEMO</dt>
 *	<dd>migemo-dict dictionary</dd>
 *  <dt>MIGEMO_DICTID_ROMA2HIRA</dt>
 *	<dd>Romaji→Hiragana conversion table</dd>
 *  <dt>MIGEMO_DICTID_HIRA2KATA</dt>
 *	<dd>Hiragana→Katakana conversion table</dd>
 *  <dt>MIGEMO_DICTID_HAN2ZEN</dt>
 *	<dd>Half-width→Full-width conversion table</dd>
 *  <dt>MIGEMO_DICTID_ZEN2HAN</dt>
 *	<dd>Full-width→Half-width conversion table</dd>
 *  </dl>
 *
 *  The return value indicates the type actually loaded, and may return the following
 *  value indicating a failure to load.
 *
 *  <dl><dt>MIGEMO_DICTID_INVALID</dt></dl>
 * @param obj Migemo object
 * @param dict_id Dictionary file type
 * @param dict_file Dictionary file path
 */
EXPORTS int MIGEMO_CALLTYPE
migemo_load(migemo *obj, int dict_id, const char *dict_file)
{
	if (!obj && dict_file)
		return MIGEMO_DICTID_INVALID;

	if (dict_id == MIGEMO_DICTID_MIGEMO) {
		/* Load migemo dictionary */
		mtree_p mtree;

		if ((mtree = load_mtree_dictionary2(obj, dict_file)) == NULL)
			return MIGEMO_DICTID_INVALID;
		obj->mtree = mtree;
		obj->enable = 1;
		return dict_id; /* Loaded successfully */
	} else {
		romaji *dict;

		switch (dict_id) {
		case MIGEMO_DICTID_ROMA2HIRA:
			/* Load Romaji dictionary */
			dict = obj->roma2hira;
			break;
		case MIGEMO_DICTID_HIRA2KATA:
			/* Load Katakana dictionary */
			dict = obj->hira2kata;
			break;
		case MIGEMO_DICTID_HAN2ZEN:
				/* Load Half-width→Full-width dictionary */
			dict = obj->han2zen;
			break;
		case MIGEMO_DICTID_ZEN2HAN:
				/* Load Full-width→Half-width dictionary */
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
 * Create a Migemo object. If successful, the object is returned as the return value;
 * if it fails, NULL is returned. The file specified by dict is loaded as a migemo-dict
 * dictionary when the object is created. If the following files exist in the same directory
 * as the dictionary:
 *
 *  <dl>
 *  <dt>roma2hira.dat</dt>
 *	<dd>Romaji→Hiragana conversion table </dd>
 *  <dt>hira2kata.dat</dt>
 *	<dd>Hiragana→Katakana conversion table </dd>
 *  <dt>han2zen.dat</dt>
 *	<dd>Half-width→Full-width conversion table </dd>
 *  </dl>
 *
 * Only those that exist will be loaded. If NULL is specified for dict, no files including
 * the dictionary will be loaded. Files can also be additionally loaded after object creation
 * using the migemo_load() function.
 * @param dict Path to migemo-dict dictionary. When NULL, the dictionary is not loaded.
 * @returns Created Migemo object
 */
EXPORTS migemo *MIGEMO_CALLTYPE
migemo_open(const char *dict)
{
	migemo *obj;

	/* Construct Migemo object and its members */
	if (!(obj = (migemo *)calloc(1, sizeof(migemo))))
		return obj;
	obj->enable = 0;
	obj->mtree = mnode_open(NULL);
	obj->charset = CHARSET_NONE;
	obj->rx = rxgen_open();
	obj->roma2hira = romaji_open();
	obj->hira2kata = romaji_open();
	obj->han2zen = romaji_open();
	obj->zen2han = romaji_open();
	if (!obj->rx || !obj->roma2hira || !obj->hira2kata || !obj->han2zen
		|| !obj->zen2han) {
		migemo_close(obj);
		return obj = NULL;
	}

	/* If a default migemo dictionary is specified, also look for romaji and katakana dictionaries */
	if (dict) {
#ifndef _MAX_PATH
#define _MAX_PATH 1024 /* Arbitrary value */
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
		dircat(h2z_dict, tmp, DICT_HAN2ZEN);
		dircat(z2h_dict, tmp, DICT_ZEN2HAN);

		mtree = load_mtree_dictionary2(obj, dict);
		if (mtree) {
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
 * Destroy the Migemo object and release resources used.
 * @param obj Migemo object to destroy
 */
EXPORTS void MIGEMO_CALLTYPE
migemo_close(migemo *obj)
{
	if (obj) {
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
 * Input the word list held by mnode into the regex generation engine.
 */
static void
migemo_query_proc(mnode *p, void *data)
{
	migemo *object = (migemo *)data;
	wordlist_p list = p->list;

	for (; list; list = list->next)
		object->addword(object, list->ptr);
}

/*
 * Prepare a buffer and make mnode write to it recursively
 */
static void
add_mnode_query(migemo *object, unsigned char *query)
{
	mnode *pnode;

	if ((pnode = mnode_query(object->mtree, query)) != NULL)
		mnode_traverse(pnode, migemo_query_proc, object);
}

/**
 * Convert input from romaji to kana and add to search keys.
 */
static int
add_roma(migemo *object, unsigned char *query)
{
	unsigned char *stop, *hira, *kata, *han;

	hira = romaji_convert(object->roma2hira, query, &stop);
	if (!stop) {
		object->addword(object, hira);
		/* Dictionary lookup using hiragana */
		add_mnode_query(object, hira);
		/* Generate katakana string and add to candidates */
		kata = romaji_convert2(object->hira2kata, hira, NULL, 0);
		object->addword(object, kata);
		/* TODO: Generate half-width kana and add to candidates */
#if 1
		han = romaji_convert2(object->zen2han, kata, NULL, 0);
		object->addword(object, han);
		/*printf("kata=%s\nhan=%s\n", kata, han);*/
		romaji_release(object->zen2han, han);
#endif
		/* Dictionary lookup using katakana */
		add_mnode_query(object, kata);
		romaji_release(object->hira2kata, kata); /* Release katakana */
	}
	romaji_release(object->roma2hira, hira); /* Release hiragana */

	return stop ? 1 : 0;
}

/**
 * Add vowels to the end of romaji and add each to the search keys.
 */
static void
add_dubious_vowels(migemo *object, unsigned char *buf, int index)
{
	const unsigned char *ptr;
	for (ptr = VOWEL_CHARS; *ptr; ++ptr) {
		buf[index] = *ptr;
		add_roma(object, buf);
	}
}

/*
 * When romaji conversion is incomplete, try complementing with [aiueo], "xn", and "xtu"
 * for conversion.
 */
static void
add_dubious_roma(migemo *object, rxgen *rx, unsigned char *query)
{
	int max;
	int len;
	char *buf;

	if (!(len = my_strlen(query)))
		return;
	/*
	 * Allocate buffer for arranging the end of romaji.
	 *     Breakdown: Original length, NUL, stuttered sound (xtu), supplementary vowels ([aieuo])
	 */
	max = len + 1 + 3 + 1;
	buf = malloc(max);
	if (buf == NULL)
		return;
	memcpy(buf, query, len);
	memset(&buf[len], 0, max - len);

	if (!strchr(VOWEL_CHARS, buf[len - 1])) {
		add_dubious_vowels(object, buf, len);
		/* If the length of the undetermined word is less than 2, or if the character before the undetermined character is a vowel... */
		if (len < 2 || strchr(VOWEL_CHARS, buf[len - 2])) {
			if (buf[len - 1] == 'n') {
				/* Try adding 'n' sound (ん) */
				memcpy(&buf[len - 1], "xn", 2);
				add_roma(object, buf);
			} else {
				/* Try adding small tsu 'っ{original consonant}{vowel}' */
				buf[len + 2] = buf[len - 1];
				memcpy(&buf[len - 1], "xtu", 3);
				add_dubious_vowels(object, buf, len + 3);
			}
		}
	}

	free(buf);
}

/*
 * Split query into segments. The segment break is usually an uppercase alphabet letter.
 * For segments that begin with multiple uppercase characters, non-uppercase characters
 * are used as delimiters.
 */
static wordlist_p
parse_query(migemo *object, const unsigned char *query)
{
	const unsigned char *curr = query;
	const unsigned char *start = NULL;
	wordlist_p querylist = NULL, *pp = &querylist;

	while (1) {
		int len, upper;
		int sum = 0;

		if (!object->char2int || (len = object->char2int(curr, NULL)) < 1)
			len = 1;
		start = curr;
		upper = (len == 1 && isupper(*curr) && isupper(curr[1]));
		curr += len;
		sum += len;
		while (1) {
			if (!object->char2int || (len = object->char2int(curr, NULL)) < 1)
				len = 1;
			if (*curr == '\0' || (len == 1 && (isupper(*curr) != 0) != upper))
				break;
			curr += len;
			sum += len;
		}
		/* Register the segment */
		if (start && start < curr) {
			*pp = wordlist_open_len(start, sum);
			pp = &(*pp)->next;
		}
		if (*curr == '\0')
			break;
	}
	return querylist;
}

/*
 * Migemo conversion for a single word. Does not check arguments.
 */
static int
query_a_word(migemo *object, unsigned char *query)
{
	unsigned char *zen;
	unsigned char *han;
	unsigned char *lower;
	int len = my_strlen(query);

	/* Of course, add the query itself to the candidates */
	object->addword(object, query);
	/* Dictionary lookup with the query itself */
	lower = malloc(len + 1);
	if (!lower)
		add_mnode_query(object, query);
	else {
		int i = 0, step;

		// Uppercase→lowercase conversion considering multibyte characters
		while (i <= len) {
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

	/* Add full-width version of query to candidates */
	zen = romaji_convert2(object->han2zen, query, NULL, 0);
	if (zen != NULL) {
		object->addword(object, zen);
		romaji_release(object->han2zen, zen);
	}

	/* Add half-width version of query to candidates */
	han = romaji_convert2(object->zen2han, query, NULL, 0);
	if (han != NULL) {
		object->addword(object, han);
		romaji_release(object->zen2han, han);
	}

	/* Add hiragana, katakana, and their dictionary lookups */
	if (add_roma(object, query))
		add_dubious_roma(object, object->rx, query);

	return 1;
}

static int
addword_rxgen(migemo *object, unsigned char *word)
{
	/* Display the word added to the regex generation engine */
	/*printf("addword_rxgen: %s\n", word);*/
	return rxgen_add(object->rx, word);
}

/**
 * Convert a string (romaji) given by query into a regular expression for Japanese search.
 * The return value is the string (regular expression) of the converted result, which must
 * be freed by passing it to the #migemo_release() function after use.
 * @param object Migemo object
 * @param query Query string
 * @returns Regular expression string. Must be released with #migemo_release().
 */
EXPORTS unsigned char *MIGEMO_CALLTYPE
migemo_query(migemo *object, const unsigned char *query)
{
	unsigned char *retval = NULL;
	wordlist_p querylist = NULL;
	wordbuf_p outbuf = NULL;

	if (object && object->rx && query) {
		wordlist_p p;

		querylist = parse_query(object, query);
		if (querylist == NULL)
			goto MIGEMO_QUERY_END; /* Error due to empty query */
		outbuf = wordbuf_open();
		if (outbuf == NULL)
			goto MIGEMO_QUERY_END; /* Error due to insufficient memory for output */

		/* Input word groups into the rxgen object and get regular expressions */
		object->addword = (MIGEMO_PROC_ADDWORD)addword_rxgen;
		rxgen_reset(object->rx);
		for (p = querylist; p; p = p->next) {
			unsigned char *answer;

			/*printf("query=%s\n", p->ptr);*/
			query_a_word(object, p->ptr);
			/* Generate search pattern (regular expression) */
			answer = rxgen_generate(object->rx);
			rxgen_reset(object->rx);
			wordbuf_cat(outbuf, answer);
			rxgen_release(object->rx, answer);
		}
	}

MIGEMO_QUERY_END:
	if (outbuf) {
		retval = outbuf->buf;
		outbuf->buf = NULL;
		wordbuf_close(outbuf);
	}
	if (querylist)
		wordlist_close(querylist);

	return retval;
}

/**
 * Release the regular expression obtained with the migemo_query() function.
 * @param p Migemo object
 * @param string Regular expression string
 */
EXPORTS void MIGEMO_CALLTYPE
migemo_release(migemo *p, unsigned char *string)
{
	free(string);
}

/**
 * Specify the meta characters (operators) used in the regular expressions generated by the
 * Migemo object. Specify which meta character with index, and replace with op. The following
 * values can be specified for index:
 *
 *  <dl>
 *  <dt>MIGEMO_OPINDEX_OR</dt>
 *	<dd>Logical OR. Default is "|" . For vim use "\|" .</dd>
 *  <dt>MIGEMO_OPINDEX_NEST_IN</dt>
 *	<dd>Opening parenthesis used for grouping. Default is "(" . In vim, "\%(" is used to
 *	prevent storing in registers \\1-\\9. In Perl, "(?:" can be used for the same purpose.</dd>
 *  <dt>MIGEMO_OPINDEX_NEST_OUT</dt>
 *	<dd>Closing parenthesis that indicates the end of grouping. Default is ")" . In vim,
 *	it's "\)" .</dd>
 *  <dt>MIGEMO_OPINDEX_SELECT_IN</dt>
 *	<dd>Opening square bracket that indicates the beginning of a selection. Default is "[" .</dd>
 *  <dt>MIGEMO_OPINDEX_SELECT_OUT</dt>
 *	<dd>Closing square bracket that indicates the end of a selection. Default is "]" .</dd>
 *  <dt>MIGEMO_OPINDEX_NEWLINE</dt>
 *	<dd>Pattern inserted between characters that "matches zero or more whitespace or newlines".
 *	Default is "" and is not set. In vim, specify "\_s*" .</dd>
 *  </dl>
 *
 * The default meta characters have the same meaning as those in Perl unless otherwise noted.
 * If the setting is successful, the return value is 1 (non-zero); if it fails, it's 0.
 * @param object Migemo object
 * @param index Meta character identifier
 * @param op Meta character string
 * @returns Non-zero on success, 0 on failure.
 */
EXPORTS int MIGEMO_CALLTYPE
migemo_set_operator(migemo *object, int index, const unsigned char *op)
{
	if (object) {
		int retval = rxgen_set_operator(object->rx, index, op);
		return retval ? 0 : 1;
	} else
		return 0;
}

/**
 * Get the meta characters (operators) used in the regular expressions generated by the
 * Migemo object. For index, refer to the migemo_set_operator() function. If the index
 * specification is correct, the return value will be a pointer to a string containing
 * the meta character; if invalid, NULL will be returned.
 * @param object Migemo object
 * @param index Meta character identifier
 * @returns Current meta character string
 */
EXPORTS const unsigned char *MIGEMO_CALLTYPE
migemo_get_operator(migemo *object, int index)
{
	return object ? rxgen_get_operator(object->rx, index) : NULL;
}

/**
 * Set up a procedure for code conversion in the Migemo object. For details about the
 * procedure, refer to MIGEMO_PROC_CHAR2INT in the "Type Reference" section.
 * @param object Migemo object
 * @param proc Procedure for code conversion
 */
EXPORTS void MIGEMO_CALLTYPE
migemo_setproc_char2int(migemo *object, MIGEMO_PROC_CHAR2INT proc)
{
	if (object)
		rxgen_setproc_char2int(object->rx, (RXGEN_PROC_CHAR2INT)proc);
}

/**
 * Set up a procedure for code conversion in the Migemo object. For details about the
 * procedure, refer to MIGEMO_PROC_INT2CHAR in the "Type Reference" section.
 * @param object Migemo object
 * @param proc Procedure for code conversion
 */
EXPORTS void MIGEMO_CALLTYPE
migemo_setproc_int2char(migemo *object, MIGEMO_PROC_INT2CHAR proc)
{
	if (object)
		rxgen_setproc_int2char(object->rx, (RXGEN_PROC_INT2CHAR)proc);
}

/**
 * Check if migemo_dict can be loaded in the Migemo object. If a valid migemo_dict can be
 * loaded and an internal conversion table can be built, it returns non-zero (TRUE);
 * if it cannot be built, it returns 0 (FALSE).
 * @param obj Migemo object
 * @returns Non-zero on success, 0 on failure.
 */
EXPORTS int MIGEMO_CALLTYPE
migemo_is_enable(migemo *obj)
{
	return obj ? obj->enable : 0;
}

#if 1
/*
 * Hidden function mainly for debugging
 */
EXPORTS void MIGEMO_CALLTYPE
migemo_print(migemo *object)
{
	if (object)
		mnode_print(object->mtree, NULL);
}
#endif
