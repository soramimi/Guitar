// Jagger -- C++ implementation of Pattern-based Japanese Morphological Analyzer
//  $Id: jagger.h 2154 2026-05-25 05:13:44Z ynaga $
// Copyright (c) 2022 Naoki Yoshinaga <ynaga@iis.u-tokyo.ac.jp>
#ifndef JAGGER_H
#define JAGGER_H
#include <algorithm>
#include <ccedar_core.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <err.h>
#include <fcntl.h>
#include <map>
#include <stdint.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define UNK_LEMMA ",*,*,*\n"

namespace jagger {
static const size_t BUF_SIZE = 1 << 18;
static const size_t CP_MAX = 0x10ffff; // limit of unicode codepoint
enum { KEY_BITS = 14,
	PAT_BITS = 7,
	FEAT_BITS = 9 }; // bits for key, pattern and features
enum { NUM = 1 << 0,
	ALPHA = 1 << 1,
	KANA = 1 << 2,
	OTHER = 0,
	ANY = 7 };
struct pattern_value_t { // packed representation for patern value
	union {
		uint64_t r;
		struct {
			uint32_t r0;
			uint32_t r1;
		};
	};
#define _(r, f, s, m)                                    \
	int f() const { return (r >> s) & ((1u << m) - 1); } \
	void f(int v)                                        \
	{                                                    \
		r &= ~(((1u << m) - 1) << s);                    \
		r |= v << s;                                     \
	}
	_(r0, shift, 25, 7)
	_(r0, ti, 16, 9) _(r0, core_fs_len, 9, 7) _(r0, fs_len, 0, 9)
		_(r1, concat, 31, 1) _(r1, ctype, 27, 4) _(r1, fs_offset, 0, 27) _(r1, fi, 0, 27)
#undef _
			pattern_value_t(int shift_ = 0, int ti_ = 0, int core_fs_len_ = 0, int fs_len_ = 0, int ctype_ = 0, int fs_offset_ = 0)
		: r(0)
	{
		shift(shift_);
		ti(ti_);
		core_fs_len(core_fs_len_);
		fs_len(fs_len_);
		ctype(ctype_);
		fs_offset(fs_offset_);
	}
};
struct pattern_info_t {
	std::string surf; // surface
	int ti_prev; // raw ti
	int count;
	pattern_value_t r;
	pattern_info_t(const std::string &surf_, int ti_prev_, int count_, int shift_, int ctype_, int fi_)
		: surf(surf_)
		, ti_prev(ti_prev_)
		, count(count_)
		, r()
	{
		r.shift(shift_), r.ctype(ctype_), r.fi(fi_);
	}
	bool operator<(const pattern_info_t &a) const
	{
		return count != a.count ? count < a.count : surf != a.surf ? surf < a.surf
																   : r.ti() > a.r.ti();
	}
	template <typename T>
	void print(FILE *writer, const T &tbag, const T &fbag) const
	{
		std::fprintf(writer, "%d\t%s%s\t%d\t%d%s", count, surf.c_str(), ti_prev == -1 ? "\t" : tbag.to_v(ti_prev).c_str(), r.shift(), r.ctype(), fbag.to_v(r.fi()).c_str());
	}
};
template <typename T>
struct bag_t { // assign unique id to key
	typedef typename std::map<T, int>::const_iterator iter;
	int to_i(const T &f)
	{
		std::pair<iter, bool> itb = _key2id.insert(std::make_pair(f, _id2key.size()));
		if (itb.second) _id2key.push_back(&itb.first->first);
		return itb.first->second;
	}
	const T &to_v(const size_t fi) const { return *_id2key[fi]; }
	size_t size() const { return _id2key.size(); }
	iter begin() const { return _key2id.begin(); }
	iter end() const { return _key2id.end(); }

private:
	typename std::map<T, int> _key2id; // can be faster with unordered_map
	std::vector<const T *> _id2key;
};
struct simple_reader {
	simple_reader(char const *begin, char const *end)
		: ptr_(begin)
		, end_(end)
	{
	} // 1 byte for sentinel
	~simple_reader() {}
	const char *ptr() const { return ptr_; }
	const char *end() const { return end_; }
	void advance(const int shift) { ptr_ += shift; }
	void operator += (int n)
	{
		advance(n);
	}

private:
	char const *ptr_;
	char const *end_;
};
struct simple_writer {
	simple_writer()
		: _buf(static_cast<char *>(std::malloc(BUF_SIZE)))
		, _p(_buf)
		, _end(_buf + BUF_SIZE)
	{
	}
	~simple_writer()
	{
		flush();
		std::free(_buf);
	}
	void flush()
	{
		for (const char *p = _buf; p < _p;) {
			const ssize_t w = ::write(1, p, static_cast<size_t>(_p - p));
			if (w > 0)
				p += w;
			else if (errno != EINTR)
				break;
		}
		_p = _buf;
	}
	template <int L> // fixed width memcpy by a factor of 2
	void write(const char *s)
	{
		std::memcpy(_p, s, L < 2 ? 1 : L < 3 ? 2
				: L < 5						 ? 4
				: L < 9						 ? 8
											 : 16),
			_p += L;
	}
	void write(const char *s, const size_t len)
	{
		len <= 64 ? std::memcpy(_p, s, 64) : len <= 128 ? std::memcpy(_p, s, 128)
														: std::memcpy(_p, s, len);
		_p += len;
	}
	bool writable(const size_t min) const { return _p + min <= _end; }

private:
	char *const _buf, *_p, *const _end;
};

// find UTF8 length from its first byte
static inline int8_t u8_len(const char *p)
{
	int8_t n = __builtin_clz(~static_cast<uint32_t>(*p) << 24);
	return n + !n;
}

// convert UTF-8 character to codepoint and proceed the pointer
static inline int byte2cp(char const **p)
{ // decode
	const uint8_t *const q = reinterpret_cast<const uint8_t *>(*p);
#define B(n, i) ((q[i] & 0x3f) << (6 * (n - i))) // fetch values in trailing bytes
	return // increment *p by the length in bytes
		(*q & 0xf0) == 0xe0 ? (*p += 3, (*q & 0xf) << 12 | B(2, 1) | B(2, 2)) : (*q & 0x80) == 0 ? (*p += 1, *q)
																			: (*q & 0xe0) == 0xc0																	 ? (*p += 2, (*q & 0x1f) << 6 | B(1, 1))
																								  : (*p += 4, (*q & 0x7) << 18 | B(3, 1) | B(3, 2) | B(3, 3));
#undef B
}

}

namespace ccedar {
struct da_ : ccedar::da_base {
	enum { VALUE_FLAG = 1U << 31,
		VALUE_MASK = ~VALUE_FLAG };
	void set_values(const jagger::pattern_value_t *const p2v, const size_t ti_max)
	{
		for (size_t i = 0; i < size(); ++i) { // use value nodes for 64bit values
			node *n(&_array[i]), *v(&_array[(n->base & VALUE_MASK) ^ 0]), *p(&_array[n->check]);
			if ((!i || (n->check >= 0 && (p->base & VALUE_MASK) != i)) // not value
				&& v->check == static_cast<int>(i)) // but a value parent
				_array[n->base] = node(p2v[v->base].r0, p2v[v->base].r1 | VALUE_FLAG),
				n->base |= VALUE_FLAG * (!i || ((p->base & VALUE_MASK) ^ i) > ti_max); // set has_value flags for non-POS patterns
		}
	}
	uint64_t longestPatternSearch(char const *p, int ti_prev, const uint16_t *const cp2i, int from = 0) const
	{ // surface longest matching + POS fallback
		const node *__restrict const array = this->_array; // localize
		for (int i(0), to(0); (i = cp2i[jagger::byte2cp(&p)]); from = to)
			if (array[to = (array[from].base & VALUE_MASK) ^ i].check != from) break;
		// ad-hoc matching at the moment; it prefers POS-ending patterns
		for (const node *n = &array[from];; n = &array[from = n->check]) {
			const int b(n->base), base(b & VALUE_MASK), base_(base ^ ti_prev);
			if (__builtin_expect(ti_prev, 1) && array[base_].check == from)
				return array[array[base_].base ^ 0].r; // POS node must have a value
			if (b & VALUE_FLAG) return array[base ^ 0].r;
		}
	}
};
}
#endif
