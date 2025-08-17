#include "GitTypes.h"
#include "common/crc32.h"

class Latin1View {
private:
	QString const &text_;
public:
	Latin1View(QString const &s)
		: text_(s)
	{
	}
	bool empty() const
	{
		return text_.isEmpty();
	}
	size_t size() const
	{
		return text_.size();
	}
	char operator [](int i) const
	{
		return (i >= 0 && i < text_.size()) ? text_.utf16()[i] : 0;
	}
};


GitHash::GitHash()
{
}

GitHash::GitHash(const std::string_view &id)
{
	assign(id);
}

GitHash::GitHash(const QString &id)
{
	assign(id);
}

GitHash::GitHash(const char *id)
{
	assign(std::string_view(id, strlen(id)));
}

template <typename VIEW> void GitHash::_assign(VIEW const &id)
{
	if (id.empty()) {
		valid_ = false;
	} else {
		valid_ = true;
		if (id.size() == GIT_ID_LENGTH) {
			char tmp[3];
			tmp[2] = 0;
			for (int i = 0; i < GIT_ID_LENGTH / 2; i++) {
				unsigned char c = id[i * 2 + 0];
				unsigned char d = id[i * 2 + 1];
				if (std::isxdigit(c) && std::isxdigit(d)) {
					tmp[0] = c;
					tmp[1] = d;
					id_[i] = strtol(tmp, nullptr, 16);
				} else {
					valid_ = false;
					break;
				}
			}
		}
	}
}

void GitHash::assign(const std::string_view &id)
{
	_assign(id);
}

void GitHash::assign(const QString &id)
{
	_assign(Latin1View(id));
}

QString GitHash::toQString(int maxlen) const
{
	if (valid_) {
		char tmp[GIT_ID_LENGTH + 1];
		for (int i = 0; i < GIT_ID_LENGTH / 2; i++) {
			sprintf(tmp + i * 2, "%02x", id_[i]);
		}
		if (maxlen < 0 || maxlen > GIT_ID_LENGTH) {
			maxlen = GIT_ID_LENGTH;
		}
		tmp[maxlen] = 0;
		return QString::fromLatin1(tmp, maxlen);
	}
	return {};
}

bool GitHash::isValid() const
{
	if (!valid_) return false;
	uint8_t c = 0;
	for (std::size_t i = 0; i < sizeof(id_); i++) {
		c |= id_[i];
	}
	return c != 0; // すべて0ならfalse
}

int GitHash::compare(const GitHash &other) const
{
	if (!valid_ && other.valid_) return -1;
	if (valid_ && !other.valid_) return 1;
	if (!valid_ && !other.valid_) return 0;
	return memcmp(id_, other.id_, sizeof(id_));
}

GitHash::operator bool() const
{
	return isValid();
}

size_t GitHash::_std_hash() const
{
	return crc::crc32(0, id_, sizeof(id_));
}

bool GitHash::isValidID(const QString &id)
{
	int zero = 0;
	int n = id.size();
	if (n >= 4 && n <= GIT_ID_LENGTH) {
		ushort const *p = id.utf16();
		for (int i = 0; i < n; i++) {
			uchar c = p[i];
			if (c == '0') {
				zero++;
			} else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
				// ok
			} else {
				return false;
			}
		}
		if (zero == GIT_ID_LENGTH) { // 全部 0 の時は false を返す
			return false;
		}
		return true; // ok
	}
	return false;
}

//

QString gitTrimPath(QString const &s)
{
	ushort const *begin = s.utf16();
	ushort const *end = begin + s.size();
	ushort const *left = begin;
	ushort const *right = end;
	while (left < right && QChar(*left).isSpace()) left++;
	while (left < right && QChar(right[-1]).isSpace()) right--;
	if (left + 1 < right && *left == '\"' && right[-1] == '\"') { // if quoted ?
		left++;
		right--;
		QByteArray ba;
		ushort const *ptr = left;
		while (ptr < right) {
			ushort c = *ptr;
			ptr++;
			if (c == '\\') {
				c = 0;
				for (int i = 0; i < 3 && ptr < right && QChar(*ptr).isDigit(); i++) { // decode \oct
					c = c * 8 + (*ptr - '0');
					ptr++;
				}
			}
			ba.push_back(c);
		}
		return QString::fromUtf8(ba);
	}
	if (left == begin && right == end) return s;
	return QString::fromUtf16((char16_t const *)left, int(right - left));
}

//

GitDiff::GitDiff(const QString &id, const QString &path, const QString &mode)
{
	makeForSingleFile(this, QString(GIT_ID_LENGTH, '0'), id, path, mode);
}

bool GitDiff::isSubmodule() const
{
	return mode == "160000";
}
