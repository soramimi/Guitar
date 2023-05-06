#include "XmlTagState.h"
#include <QString>
#include <QStringView>

void XmlTagState::push(const QString &tag)
{
	if (arr.empty()) {
		arr.reserve(100);
	}
	size_t s = arr.size();
	size_t t = tag.size();
	arr.resize(s + t + 1);
	arr[s] = '/';
	std::copy_n(tag.utf16(), t, &arr[s + 1]);
}

void XmlTagState::push(const QStringView &tag)
{
	push(tag.toString());
}

void XmlTagState::pop()
{
	size_t s = arr.size();
	while (s > 0) {
		s--;
		if (arr[s] == '/') break;
	}
	arr.resize(s);
}

bool XmlTagState::is(const char *path) const
{
	size_t s = arr.size();
	for (size_t i = 0; i < s; i++) {
		if (arr[i] != path[i]) return false;
	}
	return path[s] == 0;
}

bool XmlTagState::operator ==(const char *path) const
{
	return is(path);
}

QString XmlTagState::str() const
{
	return arr.empty() ? QString() : QString::fromUtf16((char16_t const *)&arr[0], arr.size());
}
