#include "misc.h"
#include <QPainter>
#include <QProcess>
#include <QWidget>
#include <vector>
#include "joinpath.h"



QStringList misc::splitLines(QByteArray const &ba, std::function<QString(char const *begin, size_t len)> tos)
{
	QStringList list;
	char const *begin = ba.data();
	char const *end = begin + ba.size();
	char const *ptr = begin;
	char const *left = ptr;
	while (1) {
		ushort c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (c == '\n' || c == '\r' || c == 0) {
			list.push_back(tos(left, ptr - left));
			if (c == 0) break;
			if (c == '\n') {
				ptr++;
			} else if (c == '\r') {
				ptr++;
				if (ptr < end && *ptr == '\n') {
					ptr++;
				}
			}
			left = ptr;
		} else {
			ptr++;
		}
	}
	return list;
}

QStringList misc::splitLines(const QString &text)
{
	QStringList list;
	ushort const *begin = text.utf16();
	ushort const *end = begin + text.size();
	ushort const *ptr = begin;
	ushort const *left = ptr;
	while (1) {
		ushort c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (c == '\n' || c == '\r' || c == 0) {
			list.push_back(QString::fromUtf16(left, ptr - left));
			if (c == 0) break;
			if (c == '\n') {
				ptr++;
			} else if (c == '\r') {
				ptr++;
				if (ptr < end && *ptr == '\n') {
					ptr++;
				}
			}
			left = ptr;
		} else {
			ptr++;
		}
	}
	return list;
}

void misc::splitLines(char const *begin, char const *end, std::vector<std::string> *out)
{
	char const *ptr = begin;
	char const *left = ptr;
	while (1) {
		char c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (c == '\n' || c == '\r' || c == 0) {
			out->push_back(std::string(left, ptr - left));
			if (c == 0) break;
			if (c == '\n') {
				ptr++;
			} else if (c == '\r') {
				ptr++;
				if (ptr < end && *ptr == '\n') {
					ptr++;
				}
			}
			left = ptr;
		} else {
			ptr++;
		}
	}
}

void misc::splitLines(std::string const &text, std::vector<std::string> *out)
{
	char const *begin = text.c_str();
	char const *end = begin + text.size();
	splitLines(begin, end, out);
}

QStringList misc::splitWords(const QString &text)
{
	QStringList list;
	ushort const *begin = text.utf16();
	ushort const *end = begin + text.size();
	ushort const *ptr = begin;
	ushort const *left = ptr;
	while (1) {
		ushort c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (QChar::isSpace(c) || c == 0) {
			if (left < ptr) {
				list.push_back(QString::fromUtf16(left, ptr - left));
			}
			if (c == 0) break;
			ptr++;
			left = ptr;
		} else {
			ptr++;
		}
	}
	return list;
}

QString misc::getFileName(QString const &path)
{
	int i = path.lastIndexOf('/');
	int j = path.lastIndexOf('\\');
	if (i < j) i = j;
	if (i >= 0) {
		return path.mid(i + 1);
	}
	return path;
}

QString misc::makeDateTimeString(QDateTime const &dt)
{
	if (dt.isValid()) {
		char tmp[100];
		sprintf(tmp, "%04u-%02u-%02u %02u:%02u:%02u"
				, dt.date().year()
				, dt.date().month()
				, dt.date().day()
				, dt.time().hour()
				, dt.time().minute()
				, dt.time().second()
				);
		return tmp;
	}
	return QString();
}

bool misc::starts_with(const std::string &str, const std::string &with)
{
	return strncmp(str.c_str(), with.c_str(), with.size()) == 0;
}

std::string misc::mid(const std::string &str, int start, int length)
{
	int size = (int)str.size();
	if (length < 0) length = size;

	length += start;
	if (start < 0) start = 0;
	if (length < 0) length = 0;
	if (start > size) start = size;
	if (length > size) length = size;
	length -= start;

	return std::string(str.c_str() + start, length);
}

#ifdef _WIN32
QString misc::normalizePathSeparator(QString const &str)
{
	if (!str.isEmpty()) {
		ushort const *s = str.utf16();
		size_t n = str.size();
		std::vector<ushort> v;
		v.reserve(n);
		for (size_t i = 0; i < n; i++) {
			ushort c = s[i];
			if (c == '/') {
				c = '\\';
			}
			v.push_back(c);
		}
		ushort const *p = &v[0];
		return QString::fromUtf16(p, n);
	}
	return QString();
}
#else
QString misc::normalizePathSeparator(QString const &s)
{
	return s;
}
#endif

QString misc::joinWithSlash(QString const &left, QString const &right)
{
	if (!left.isEmpty() && !right.isEmpty()) {
		return joinpath(left, right);
	}
	return !left.isEmpty() ? left : right;
}

int misc::runCommand(QString const &cmd, QByteArray *out)
{
	out->clear();
	QProcess proc;
	proc.start(cmd);
	proc.waitForStarted();
	proc.closeWriteChannel();
	proc.setReadChannel(QProcess::StandardOutput);
	while (1) {
		QProcess::ProcessState s = proc.state();
		if (proc.waitForReadyRead(1)) {
			char tmp[1024];
			qint64 len = proc.read(tmp, sizeof(tmp));
			if (len < 1) break;
			out->append(tmp, len);
		} else if (s == QProcess::NotRunning) {
			break;
		}
	}

	return proc.exitCode();
}

void misc::setFixedSize(QWidget *w)
{
	Qt::WindowFlags flags = w->windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags |= Qt::MSWindowsFixedSizeDialogHint;
	w->setWindowFlags(flags);
	w->setFixedSize(w->size());
}

void misc::drawFrame(QPainter *pr, int x, int y, int w, int h, const QColor &color)
{
	if (w < 3 || h < 3) {
		if (w > 0 && h > 0) {
			pr->fillRect(x, y, w, h, color);
		}
	} else {
		pr->fillRect(x, y, w - 1, 1, color);
		pr->fillRect(x, y + 1, 1, h -1, color);
		pr->fillRect(x + w - 1, y, 1, h -1, color);
		pr->fillRect(x + 1, y + h - 1, w - 1, 1, color);
	}
}




