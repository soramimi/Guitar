
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

void clear_state(utf8_reader_state_t *s)
{
	s->a = 0;
	s->b = 0;
}

int decode_utf8(utf8_reader_state_t *state, uint8_t c)
{
	if (c & 0x80) {
		if (c & 0x40) {
			state->a = c;
			if (c & 0x20) {
				if (c & 0x10) {
					if (c & 0x08) {
						if (c & 0x04) {
							state->b = c & 0x01;
						} else {
							state->b = c & 0x03;
						}
					} else {
						state->b = c & 0x07;
					}
				} else {
					state->b = c & 0x0f;
				}
			} else {
				state->b = c & 0x1f;
			}
			return -1;
		} else {
			state->a <<= 1;
			state->b = (state->b << 6) | (c & 0x3f);
			if ((state->a & 0x40) == 0) {
				return state->b;
			}
			return -1;
		}
	} else {
		state->a = 0;
		state->b = 0;
		return c;
	}
}

void encode_utf8(writer8 *writer, uint32_t code)
{
	if (code < 0x80) {
		writer->put(code);
	} else if (code < 0x800) {
		writer->put((code >> 6) | 0xc0);
		writer->put((code & 0x3f) | 0x80);
	} else if (code < 0x10000) {
		writer->put((code >> 12) | 0xe0);
		writer->put(((code >> 6) & 0x3f) | 0x80);
		writer->put((code & 0x3f) | 0x80);
	} else if (code < 0x200000) {
		writer->put((code >> 18) | 0xf0);
		writer->put(((code >> 12) & 0x3f) | 0x80);
		writer->put(((code >> 6) & 0x3f) | 0x80);
		writer->put((code & 0x3f) | 0x80);
	} else if (code < 0x4000000) {
		writer->put((code >> 24) | 0xf8);
		writer->put(((code >> 18) & 0x3f) | 0x80);
		writer->put(((code >> 12) & 0x3f) | 0x80);
		writer->put(((code >> 6) & 0x3f) | 0x80);
		writer->put((code & 0x3f) | 0x80);
	} else {
		writer->put((code >> 30) | 0xfc);
		writer->put(((code >> 24) & 0x3f) | 0x80);
		writer->put(((code >> 18) & 0x3f) | 0x80);
		writer->put(((code >> 12) & 0x3f) | 0x80);
		writer->put(((code >> 6) & 0x3f) | 0x80);
		writer->put((code & 0x3f) | 0x80);
	}
}

void encode_utf16(writer16 *writer, uint32_t code)
{
	if (code >= 0x010000 && code <= 0x10ffff) {
		uint16_t hi = (code - 0x10000) / 0x400 + 0xD800;
		uint16_t lo = (code - 0x10000) % 0x400 + 0xDC00;
		writer->put(hi);
		writer->put(lo);
		return;
	}
	writer->put(code);
}

//

utf8decoder::utf8decoder(char const *begin, char const *end)
	: begin(begin)
	, end(end)
	, pos(0)
{
	clear_state(&s);
}

uint32_t utf8decoder::next()
{
	while (begin + pos < end) {
		int c = decode_utf8(&s, (uint8_t)begin[pos]);
		pos++;
		if (c >= 0) return c;
	}
	return 0;
}

} // namespace


//

utf32::utf32(const uint32_t *ptr, const uint32_t *end)
{
	data.ptr = ptr;
	data.end = end;
}

utf32::utf32(const uint32_t *ptr)
{
	data.ptr = ptr;
	for (data.end = ptr; *data.end; data.end++);
}

utf32::utf32(const uint32_t *ptr, size_t len)
{
	data.ptr = ptr;
	data.end = ptr + len;
}

uint32_t utf32::next()
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

uint32_t utf16::next()
{
	if (data.ptr && data.ptr < data.end) {
		uint32_t code = *data.ptr++;
		if (code >= 0xd800 && code < 0xdc00) {
			if (data.ptr < data.end) {
				uint32_t low = *data.ptr;
				if (low >= 0xdc00 && low < 0xe000) {
					code = 0x10000 + (code - 0xd800) * 0x0400 + (low - 0xdc00);
					data.ptr++;
				}
			}
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
		dst[len++] = c;
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

uint32_t utf8::next()
{
	return reader.next();
}


