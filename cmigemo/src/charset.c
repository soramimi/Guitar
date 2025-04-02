/* vim:set ts=8 sts=4 sw=4 tw=0: */
/*
 * charset.c -
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 * Last Change: 20-Sep-2009.
 */

#define BUFLEN_DETECT 4096

#include "charset.h"
#include <limits.h>
#include <stdio.h>

int cp932_char2int(const unsigned char *in, unsigned int *out)
{
	if (((0x81 <= in[0] && in[0] <= 0x9f)
			|| (0xe0 <= in[0] && in[0] <= 0xf0))
		&& ((0x40 <= in[1] && in[1] <= 0x7e)
			|| (0x80 <= in[1] && in[1] <= 0xfc))) {
		if (out)
			*out = (unsigned int)in[0] << 8 | (unsigned int)in[1];
		return 2;
	} else {
		if (out)
			*out = in[0];
		return 1;
	}
}

int cp932_int2char(unsigned int in, unsigned char *out)
{
	if (in >= 0x100) {
		if (out) {
			out[0] = (unsigned char)((in >> 8) & 0xFF);
			out[1] = (unsigned char)(in & 0xFF);
		}
		return 2;
	} else
		return 0;
}

#define IS_EUC_RANGE(c) (0xa1 <= (c) && (c) <= 0xfe)

int eucjp_char2int(const unsigned char *in, unsigned int *out)
{
	if ((in[0] == 0x8e && 0xa0 <= in[1] && in[1] <= 0xdf)
		|| (IS_EUC_RANGE(in[0]) && IS_EUC_RANGE(in[1]))) {
		if (out)
			*out = (unsigned int)in[0] << 8 | (unsigned int)in[1];
		return 2;
	} else {
		if (out)
			*out = in[0];
		return 1;
	}
}

int eucjp_int2char(unsigned int in, unsigned char *out)
{
	/* The content is the same as CP932, but kept separate for future JISX0213 support */
	if (in >= 0x100) {
		if (out) {
			out[0] = (unsigned char)((in >> 8) & 0xFF);
			out[1] = (unsigned char)(in & 0xFF);
		}
		return 2;
	} else
		return 0;
}

static int
utf8_char2int_noascii(const unsigned char *in, unsigned int *out)
{
	int len = 0;
	int i;
	unsigned int ch;

	for (ch = in[0]; ch & 0x80; ch <<= 1)
		++len;
	/*printf("len=%d in=%s\n", len, in);*/
	if (len < 2)
		return 0;
	ch = (ch & 0xff) >> len;
	for (i = 1; i < len; ++i) {
		if ((in[i] & 0xc0) != 0x80)
			return 0;
		ch <<= 6;
		ch += in[i] & 0x3f;
	}
	/*printf("len=%d in=%s ch=%08x\n", len, in, ch);*/
	if (out)
		*out = ch;
	return len;
}

int utf8_char2int(const unsigned char *in, unsigned int *out)
{
	int retval = utf8_char2int_noascii(in, out);
	if (retval)
		return retval;
	else {
		if (out)
			*out = in[0];
		return 1;
	}
}

int utf8_int2char(unsigned int in, unsigned char *out)
{
	if (in < 0x80)
		return 0;
	if (in < 0x800) {
		if (out) {
			out[0] = 0xc0 + (in >> 6);
			out[1] = 0x80 + ((in >> 0) & 0x3f);
		}
		return 2;
	}
	if (in < 0x10000) {
		if (out) {
			out[0] = 0xe0 + (in >> 12);
			out[1] = 0x80 + ((in >> 6) & 0x3f);
			out[2] = 0x80 + ((in >> 0) & 0x3f);
		}
		return 3;
	}
	if (in < 0x200000) {
		if (out) {
			out[0] = 0xf0 + (in >> 18);
			out[1] = 0x80 + ((in >> 12) & 0x3f);
			out[2] = 0x80 + ((in >> 6) & 0x3f);
			out[3] = 0x80 + ((in >> 0) & 0x3f);
		}
		return 4;
	}
	if (in < 0x4000000) {
		if (out) {
			out[0] = 0xf8 + (in >> 24);
			out[1] = 0x80 + ((in >> 18) & 0x3f);
			out[2] = 0x80 + ((in >> 12) & 0x3f);
			out[3] = 0x80 + ((in >> 6) & 0x3f);
			out[4] = 0x80 + ((in >> 0) & 0x3f);
		}
		return 5;
	} else {
		if (out) {
			out[0] = 0xf8 + (in >> 30);
			out[1] = 0x80 + ((in >> 24) & 0x3f);
			out[2] = 0x80 + ((in >> 18) & 0x3f);
			out[3] = 0x80 + ((in >> 12) & 0x3f);
			out[4] = 0x80 + ((in >> 6) & 0x3f);
			out[5] = 0x80 + ((in >> 0) & 0x3f);
		}
		return 6;
	}
}

int charset_detect_buf(const unsigned char *buf, int len)
{
	int sjis = 0, smode = 0;
	int euc = 0, emode = 0, eflag = 0;
	int utf8 = 0, umode = 0, ufailed = 0;
	int i;
	for (i = 0; i < len; ++i) {
		unsigned char c = buf[i];
		// Check if it's SJIS
		if (smode) {
			if ((0x40 <= c && c <= 0x7e) || (0x80 <= c && c <= 0xfc))
				++sjis;
			smode = 0;
		} else if ((0x81 <= c && c <= 0x9f) || (0xe0 <= c && c <= 0xf0))
			smode = 1;
		// Check if it's EUC
		eflag = 0xa1 <= c && c <= 0xfe;
		if (emode) {
			if (eflag)
				++euc;
			emode = 0;
		} else if (eflag)
			emode = 1;
		// Check if it's UTF8
		if (!ufailed) {
			if (umode < 1) {
				if ((c & 0x80) != 0) {
					if ((c & 0xe0) == 0xc0)
						umode = 1;
					else if ((c & 0xf0) == 0xe0)
						umode = 2;
					else if ((c & 0xf8) == 0xf0)
						umode = 3;
					else if ((c & 0xfc) == 0xf8)
						umode = 4;
					else if ((c & 0xfe) == 0xfc)
						umode = 5;
					else {
						ufailed = 1;
						--utf8;
					}
				}
			} else {
				if ((c & 0xc0) == 0x80) {
					++utf8;
					--umode;
				} else {
					--utf8;
					umode = 0;
					ufailed = 1;
				}
			}
			if (utf8 < 0)
				utf8 = 0;
		}
	}
	// Return the encoding with the highest score in the end
	if (euc > sjis && euc > utf8)
		return CHARSET_EUCJP;
	else if (!ufailed && utf8 > euc && utf8 > sjis)
		return CHARSET_UTF8;
	else if (sjis > euc && sjis > utf8)
		return CHARSET_CP932;
	else
		return CHARSET_NONE;
}

void charset_getproc(int charset, CHARSET_PROC_CHAR2INT *char2int,
	CHARSET_PROC_INT2CHAR *int2char)
{
	CHARSET_PROC_CHAR2INT c2i = NULL;
	CHARSET_PROC_INT2CHAR i2c = NULL;
	switch (charset) {
	case CHARSET_CP932:
		c2i = cp932_char2int;
		i2c = cp932_int2char;
		break;
	case CHARSET_EUCJP:
		c2i = eucjp_char2int;
		i2c = eucjp_int2char;
		break;
	case CHARSET_UTF8:
		c2i = utf8_char2int;
		i2c = utf8_int2char;
		break;
	default:
		break;
	}
	if (char2int)
		*char2int = c2i;
	if (int2char)
		*int2char = i2c;
}

int charset_detect_file(const char *path)
{
	int charset = CHARSET_NONE;
	FILE *fp;
	if ((fp = fopen(path, "rt")) != NULL) {
		unsigned char buf[BUFLEN_DETECT];
		size_t len = fread(buf, sizeof(buf[0]), sizeof(buf), fp);
		fclose(fp);
		if (len > 0 && len <= INT_MAX)
			charset = charset_detect_buf(buf, (int)len);
	}
	return charset;
}
