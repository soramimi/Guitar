
#include "unicode.h"

namespace unicode_helper_ {

class reader {
public:
	virtual ~reader() = default;
	virtual int get() = 0;
};

class writer8 {
public:
	virtual ~writer8() = default;
	virtual void put(int c) = 0;
};

class writer16 {
public:
	virtual ~writer16() = default;
	virtual void put(int c) = 0;
};

namespace {

constexpr uint32_t REPLACEMENT_CHARACTER = 0xFFFD;
constexpr uint32_t MAX_CODE_POINT = 0x10FFFF;

bool is_surrogate(uint32_t code)
{
	return code >= 0xD800 && code <= 0xDFFF;
}

// codeがUnicodeのスカラー値として妥当か(サロゲート・範囲外を除外)
bool is_valid_scalar_value(uint32_t code)
{
	return code <= MAX_CODE_POINT && !is_surrogate(code);
}

} // namespace

// RFC 3629 / WHATWG Encoding Standard に準拠したUTF-8デコード。
// 不正な先頭バイト、途中で終端したシーケンス、継続バイトの不足・不正、
// 冗長エンコーディング(overlong)、サロゲート、U+10FFFF超過は
// すべて置換文字 U+FFFD として扱い、先頭バイト1つ分だけ読み飛ばして再同期する。
// 戻り値の 0 は「(誤り検出ではなく)バッファの終端に達した」ことのみを意味する。
uint32_t decode_utf8(char const *begin, char const *end, size_t *pos)
{
	size_t avail = (size_t)(end - begin) - *pos;
	if (avail == 0) return 0; // 正真のバッファ終端

	uint8_t c0 = (uint8_t)begin[*pos];

	if (c0 < 0x80) { // ASCII (U+0000 も含む)
		(*pos)++;
		return c0;
	}

	int len;
	uint32_t min_code;
	uint32_t code;
	if ((c0 & 0xE0) == 0xC0) {
		len = 2; min_code = 0x80; code = c0 & 0x1F;
	} else if ((c0 & 0xF0) == 0xE0) {
		len = 3; min_code = 0x800; code = c0 & 0x0F;
	} else if ((c0 & 0xF8) == 0xF0) {
		len = 4; min_code = 0x10000; code = c0 & 0x07;
	} else {
		// 継続バイト単独(0x80-0xBF)や、RFC 3629 で廃止された5/6バイト形式(0xF8-0xFF)
		(*pos)++;
		return REPLACEMENT_CHARACTER;
	}

	if (avail < (size_t)len) {
		// バッファ終端でシーケンスが途切れている
		*pos += avail;
		return REPLACEMENT_CHARACTER;
	}

	for (int i = 1; i < len; i++) {
		uint8_t cc = (uint8_t)begin[*pos + i];
		if ((cc & 0xC0) != 0x80) {
			// 継続バイトが不正。先頭バイト1つだけ読み飛ばして再同期する
			(*pos)++;
			return REPLACEMENT_CHARACTER;
		}
		code = (code << 6) | (cc & 0x3F);
	}
	*pos += (size_t)len;

	if (code < min_code) return REPLACEMENT_CHARACTER; // 冗長エンコーディング(overlong)
	if (!is_valid_scalar_value(code)) return REPLACEMENT_CHARACTER; // サロゲート or 範囲外

	return code;
}

void encode_utf8(uint32_t code, std::function<void (char)> put)
{
	if (!is_valid_scalar_value(code)) {
		code = REPLACEMENT_CHARACTER; // サロゲートやU+10FFFF超過はそのまま出力せず置換文字にする
	}
	if (code < 0x80) {
		put((char)code);
	} else if (code < 0x800) {
		put((char)((code >> 6) | 0xc0));
		put((char)((code & 0x3f) | 0x80));
	} else if (code < 0x10000) {
		put((char)((code >> 12) | 0xe0));
		put((char)(((code >> 6) & 0x3f) | 0x80));
		put((char)((code & 0x3f) | 0x80));
	} else { // <= 0x10FFFF (is_valid_scalar_valueで保証済み)。RFC 3629によりUTF-8は最大4バイト
		put((char)((code >> 18) | 0xf0));
		put((char)(((code >> 12) & 0x3f) | 0x80));
		put((char)(((code >> 6) & 0x3f) | 0x80));
		put((char)((code & 0x3f) | 0x80));
	}
}

void encode_utf8(writer8 *writer, uint32_t code)
{
	encode_utf8(code, [&](char c) { writer->put(c); });
}

void encode_utf16(writer16 *writer, uint32_t code)
{
	if (!is_valid_scalar_value(code)) {
		code = REPLACEMENT_CHARACTER; // サロゲートやU+10FFFF超過はそのまま出力せず置換文字にする
	}
	if (code >= 0x010000) {
		uint16_t hi = (uint16_t)((code - 0x10000) / 0x400 + 0xD800);
		uint16_t lo = (uint16_t)((code - 0x10000) % 0x400 + 0xDC00);
		writer->put(hi);
		writer->put(lo);
		return;
	}
	writer->put((uint16_t)code);
}

//

utf8decoder::utf8decoder(char const *begin, char const *end)
	: begin(begin)
	, end(end)
	, pos(0)
{
}

uint32_t utf8decoder::next()
{
	return decode_utf8(begin, end, &pos);
}

} // namespace


//

utf32::utf32(const char32_t *ptr, const char32_t *end)
{
	data.ptr = ptr;
	data.end = end;
}

utf32::utf32(const char32_t *ptr)
{
	data.ptr = ptr;
	for (data.end = ptr; *data.end; data.end++);
}

utf32::utf32(const char32_t *ptr, size_t len)
{
	data.ptr = ptr;
	data.end = ptr + len;
}

char32_t utf32::next()
{
	if (data.ptr && data.ptr < data.end) {
		return *data.ptr++;
	}
	return 0;
}

//

utf16::utf16(const uint16_t *ptr, const uint16_t *end)
{
	data.ptr = ptr;
	data.end = end;
}

utf16::utf16(const uint16_t *ptr)
{
	data.ptr = ptr;
	for (data.end = ptr; *data.end; data.end++);
}

utf16::utf16(const uint16_t *ptr, size_t len)
{
	data.ptr = ptr;
	data.end = ptr + len;
}

char32_t utf16::next()
{
	if (data.ptr && data.ptr < data.end) {
		uint32_t code = *data.ptr++;
		if (code >= 0xd800 && code < 0xdc00) { // 上位サロゲート
			if (data.ptr < data.end) {
				uint32_t low = *data.ptr;
				if (low >= 0xdc00 && low < 0xe000) {
					code = 0x10000 + (code - 0xd800) * 0x0400 + (low - 0xdc00);
					data.ptr++;
					return code;
				}
			}
			return 0xFFFD; // 対になる下位サロゲートが無い(バッファ終端 or 不正な並び)
		}
		if (code >= 0xdc00 && code < 0xe000) {
			return 0xFFFD; // 単独の下位サロゲート(不正な並び)
		}
		return code;
	}
	return 0;
}

//

class utf8encoder::internal_writer : public unicode_helper_::writer8 {
public:
	char *dst;
	int len;
public:
	internal_writer(char *p)
		: dst(p)
		, len(0)
	{
	}
	~internal_writer() override = default;
	void put(int c) override
	{
		dst[len++] = (char)c;
	}
};

utf8encoder::utf8encoder(abstract_unicode_reader *reader)
{
	set(reader);
}

void utf8encoder::set(abstract_unicode_reader *reader)
{
	data.reader = reader;
	data.len = 0;
	data.pos = 0;
}

bool utf8encoder::next_()
{
	if (data.pos < data.len) {
		data.c = data.buf[data.pos];
		data.pos++;
		return true;
	}
	data.c = 0;
	return false;
}

bool utf8encoder::next()
{
	if (next_()) {
		return true;
	}
	if (data.reader) {
		uint32_t code = data.reader->next();
		if (code) {
			internal_writer w(data.buf);
			unicode_helper_::encode_utf8(&w, code);
			data.len = w.len;
			data.pos = 0;
			return next_();
		}
	}
	return false;
}

char utf8encoder::get()
{
	if (next()) {
		return data.c;
	}
	return 0;
}

int utf8encoder::pos() const
{
	return data.pos;
}

//

class utf16encoder::internal_writer : public unicode_helper_::writer16 {
public:
	uint16_t *dst;
	int len = 0;
public:
	internal_writer(uint16_t *p)
		: dst(p)
	{
	}
	~internal_writer() override = default;
	void put(int c) override
	{
		dst[len++] = c;
	}
};

utf16encoder::utf16encoder(abstract_unicode_reader *reader)
{
	set(reader);
}

void utf16encoder::set(abstract_unicode_reader *reader)
{
	data.reader = reader;
	data.len = 0;
	data.pos = 0;
}

bool utf16encoder::next_()
{
	if (data.pos < data.len) {
		data.c = data.buf[data.pos];
		data.pos++;
		return true;
	}
	data.c = 0;
	return false;
}

bool utf16encoder::next()
{
	if (next_()) {
		return true;
	}
	if (data.reader) {
		uint32_t code = data.reader->next();
		if (code) {
			internal_writer w(data.buf);
			unicode_helper_::encode_utf16(&w, code);
			data.len = w.len;
			data.pos = 0;
			return next_();
		}
	}
	return false;
}

uint16_t utf16encoder::get()
{
	if (next()) {
		return data.c;
	}
	return 0;
}

//

utf8::utf8(char const *ptr, char const *end)
	: reader(ptr, end)
{
}

utf8::utf8(char const *ptr)
	: reader(ptr, ptr + strlen(ptr))
{
}

utf8::utf8(char const *ptr, size_t len)
	: reader(ptr, ptr + len)
{
}

char32_t utf8::next()
{
	return reader.next();
}


