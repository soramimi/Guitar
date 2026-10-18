#ifndef XSTRING_H
#define XSTRING_H

#include <string.h>
#include <stdlib.h>
#include <string_view>
#include <utility>
#include <algorithm>

class xstring {
private:
	struct core_t {
		unsigned int ref_ = 0;
		size_t capacity = 0;
		size_t size = 0;
		char chars[1];
	};
	core_t *core = nullptr;
	
	// 確保失敗・サイズ超過は回復不能として異常終了する
	[[noreturn]] static void _fatal()
	{
		abort();
	}
	void *_malloc(size_t n)
	{
		void *p = malloc(n);
		if (!p) _fatal();
		return p;
	}
	void _free(void *p)
	{
		free(p);
	}
	core_t *_alloc(size_t capacity)
	{
		if (capacity > (size_t)-1 - sizeof(core_t)) _fatal();
		core_t *e = (core_t *)_malloc(sizeof(core_t) + capacity);
		e->ref_ = 0;
		e->capacity = capacity;
		e->size = 0;
		e->chars[0] = '\0';
		return e;
	}
	// 容量不足で拡張するとき、少なくとも現容量の倍を確保する(償却 O(1) の追記のため)
	size_t _grow(size_t needed) const
	{
		size_t cap = core ? core->capacity : 0;
		if (cap > (size_t)-1 / 2) return needed;
		return std::max(needed, cap * 2);
	}
	void _attach(core_t *e)
	{
		if (core != e) {
			if (core) {
				if (core->ref_ > 1) {
					core->ref_--;
				} else {
					_free(core);
				}
			}
			core = e;
			if (core) {
				core->ref_++;
			}
		}
	}
public:
	xstring()
	{
	}
	~xstring()
	{
		clear();
	}
	xstring(char const *p)
	{
		if (p) assign(std::string_view(p));
	}
	xstring(std::string_view sv)
	{
		assign(sv);
	}
	xstring(xstring &&other)
	{
		std::swap(core, other.core);
	}
	xstring(xstring const &other)
	{
		_attach(other.core);
	}
	xstring &operator = (xstring &&other)
	{
		if (this != &other) {
			std::swap(core, other.core);
			other.clear();
		}
		return *this;
	}
	xstring &operator = (xstring const &other)
	{
		_attach(other.core);
		return *this;
	}
	xstring &operator = (std::string_view sv)
	{
		assign(sv);
		return *this;
	}
	size_t size() const
	{
		return core ? core->size : 0;
	}
	void clear()
	{
		_attach(nullptr);
	}
	bool empty() const
	{
		return size() == 0;
	}
	// 書き込み用: 共有されていれば分離(copy-on-write)する
	char *data()
	{
		if (!core || core->ref_ > 1) {
			resize(size());
		}
		return core->chars;
	}
	char const *data() const
	{
		return core ? core->chars : "";
	}
	char const *c_str() const
	{
		return data();
	}
	void assign(std::string_view s)
	{
		if (!s.empty()) {
			size_t n = s.size();
			core_t *new_d = _alloc(n);
			memcpy(new_d->chars, s.data(), n);
			new_d->chars[n] = '\0';
			new_d->size = n;
			_attach(new_d);
		} else {
			clear();
		}
	}
	void resize(size_t n)
	{
		if (!core || core->ref_ > 1 || core->capacity < n) {
			core_t *new_d = _alloc(core && core->capacity < n ? _grow(n) : n);
			if (core) {
				new_d->size = core->size < n ? core->size : n;
				memcpy(new_d->chars, core->chars, new_d->size);
			}
			_attach(new_d); // 旧 core の解放は _attach に任せる
		}
		core->size = n;
		core->chars[n] = '\0';
	}
	size_t capacity() const
	{
		return core ? core->capacity : 0;
	}
	// 少なくとも n 文字分の容量を確保する(内容とサイズは保持。共有されていれば分離する)
	void reserve(size_t n)
	{
		if (core && core->ref_ == 1 && core->capacity >= n) return;
		if (!core && n == 0) return;
		size_t sz = size();
		core_t *e = _alloc(n < sz ? sz : n);
		if (sz > 0) memcpy(e->chars, core->chars, sz);
		e->chars[sz] = '\0';
		e->size = sz;
		_attach(e);
	}
	std::string_view view() const
	{
		return std::string_view(data(), size());
	}
	xstring &append(std::string_view s)
	{
		size_t n = size();
		size_t m = s.size();
		if (m == 0) return *this;
		if (m > (size_t)-1 - n) _fatal();
		if (core && core->ref_ == 1 && core->capacity >= n + m) {
			// 追記先は既存データの後ろなので、s が自身の一部でも重ならない
			memcpy(core->chars + n, s.data(), m);
			core->size = n + m;
			core->chars[n + m] = '\0';
		} else {
			// 旧 core は _attach で解放されるので、s のコピーは attach の前に済ませる
			core_t *e = _alloc(core && core->capacity < n + m ? _grow(n + m) : n + m);
			if (n > 0) memcpy(e->chars, core->chars, n);
			memcpy(e->chars + n, s.data(), m);
			e->chars[n + m] = '\0';
			e->size = n + m;
			_attach(e);
		}
		return *this;
	}
	static constexpr size_t npos = (size_t)-1;
	// pos から最大 len 文字の部分文字列を返す(pos が範囲外なら空)
	xstring substr(size_t pos, size_t len = npos) const
	{
		xstring r;
		size_t n = size();
		if (pos < n) {
			if (len > n - pos) len = n - pos;
			core_t *e = r._alloc(len);
			memcpy(e->chars, core->chars + pos, len);
			e->chars[len] = '\0';
			e->size = len;
			r._attach(e);
		}
		return r;
	}
	size_t find(std::string_view s, size_t pos = 0) const
	{
		return view().find(s, pos);
	}
	int compare(std::string_view s) const
	{
		size_t n = std::min(size(), s.size());
		int i = memcmp(data(), s.data(), n);
		if (i != 0) return i;
		if (size() < s.size()) return -1;
		if (size() > s.size()) return 1;
		return 0;
	}
	operator std::string_view () const
	{
		return view();
	}
	bool operator == (std::string_view b) const
	{
		return compare(b) == 0;
	}
	bool operator != (std::string_view b) const
	{
		return compare(b) != 0;
	}
#ifdef QT_VERSION
	static inline xstring fromQString(QString const &a)
	{
		QByteArray ba = a.toUtf8();
		return xstring(std::string_view(ba.data(), ba.size()));
	}
#endif
};

#ifdef QT_VERSION
static inline QString toQString(xstring const &a)
{
	return QString::fromUtf8(a.data(), a.size());
}
static inline xstring fromQString(QString const &a)
{
	QByteArray ba = a.toUtf8();
	return xstring(std::string_view(ba.data(), ba.size()));
}
#endif

// std::string はメモリ確保を伴うため、必要な場合のみ XSTRING_WITH_STD_STRING を定義して有効にする
#ifdef XSTRING_WITH_STD_STRING
#include <string>
static inline std::string to_std_string(xstring const &a)
{
	return std::string(a.data(), a.size());
}
#endif

static inline std::string_view to_std_string_view(xstring const &a)
{
	return std::string_view(a.data(), a.size());
}

#endif // XSTRING_H
