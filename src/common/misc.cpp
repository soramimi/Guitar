#include "common/misc.h"
#include <QDebug>
#include <QFileInfo>
#include <QPainter>
#include <QProcess>
#include <QWidget>
#include <QContextMenuEvent>
#include <vector>
#include "common/joinpath.h"

QString misc::getApplicationDir()
{
	QString path = QApplication::applicationFilePath();
	int i = path.lastIndexOf('\\');
	int j = path.lastIndexOf('/');
	if (i < j) i = j;
	if (i > 0) path = path.mid(0, i);
	return path;
}

QStringList misc::splitLines(QByteArray const &ba, std::function<QString(char const *ptr, size_t len)> tos)
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

void misc::splitLines(char const *begin, char const *end, std::vector<std::string> *out, bool need_crlf)
{
	char const *ptr = begin;
	char const *left = ptr;
	while (1) {
		char c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (c == '\n' || c == '\r' || c == 0) {
			char const *end = ptr;
			if (c == '\n') {
				ptr++;
			} else if (c == '\r') {
				ptr++;
				if (ptr < end && *ptr == '\n') {
					ptr++;
				}
			}
			if (need_crlf) {
				end = ptr;
			}
			out->push_back(std::string(left, end - left));
			if (c == 0) break;
			left = ptr;
		} else {
			ptr++;
		}
	}
}

void misc::splitLines(std::string const &text, std::vector<std::string> *out, bool need_crlf)
{
	char const *begin = text.c_str();
	char const *end = begin + text.size();
	splitLines(begin, end, out, need_crlf);
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
	proc.setReadChannel(QProcess::StandardOutput);
	proc.start(cmd);
	proc.waitForStarted();
	proc.closeWriteChannel();
	while (1) {
		QProcess::ProcessState s = proc.state();
		if (proc.waitForReadyRead(1)) {
			while (1) {
				char tmp[1024];
				qint64 len = proc.read(tmp, sizeof(tmp));
				if (len < 1) break;
				out->append(tmp, len);
			}
		} else if (s == QProcess::NotRunning) {
			break;
		}
	}

	return proc.exitCode();
}

int misc::runCommand(QString const &cmd, QByteArray const *in, QByteArray *out)
{
	out->clear();
	if (in->isEmpty()) {
		return -1;
	}
	QProcess proc;
	proc.setReadChannel(QProcess::StandardOutput);
	proc.start(cmd);
	proc.waitForStarted();
	{
		char const *p = in->data();
		int n = in->size();
		if (n > 65536) {
			n = 65536;
		}
		proc.write(p, n);
	}
	proc.closeWriteChannel();
	while (1) {
		QProcess::ProcessState s = proc.state();
		if (proc.waitForReadyRead(1)) {
			while (1) {
				char tmp[1024];
				qint64 len = proc.read(tmp, sizeof(tmp));
				if (len < 1) break;
				out->append(tmp, len);
			}
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

void misc::drawFrame(QPainter *pr, int x, int y, int w, int h, QColor color_topleft, QColor color_bottomright)
{
	if (w < 3 || h < 3) {
		if (w > 0 && h > 0) {
			pr->fillRect(x, y, w, h, color_topleft);
		}
	} else {
		if (!color_topleft.isValid()) color_topleft = Qt::black;
		if (!color_bottomright.isValid()) color_bottomright = color_topleft;
		pr->fillRect(x, y, w - 1, 1, color_topleft);
		pr->fillRect(x, y + 1, 1, h -1, color_topleft);
		pr->fillRect(x + w - 1, y, 1, h -1, color_bottomright);
		pr->fillRect(x + 1, y + h - 1, w - 1, 1, color_bottomright);
	}
}

void misc::dump(uint8_t const *ptr, size_t len)
{
	if (ptr && len > 0) {
		size_t pos = 0;
		while (pos < len) {
			char tmp[100];
			char *dst = tmp;
			sprintf(dst, "%08llX ", ((unsigned long long)(pos)));
			dst += 9;
			for (int i = 0; i < 16; i++) {
				if (pos + i < len) {
					sprintf(dst, "%02X ", ptr[pos + i] & 0xff);
				} else {
					sprintf(dst, "   ");
				}
				dst += 3;
			}
			for (int i = 0; i < 16; i++) {
				int c = ' ';
				if (pos < len) {
					c = ptr[pos] & 0xff;
					if (!isprint(c)) {
						c = '.';
					}
					pos++;
				}
				*dst = c;
				dst++;
			}
			*dst = 0;
			qDebug() << tmp;
		}
	}
}

void misc::dump(const QByteArray *in)
{
	size_t len = 0;
	uint8_t const *ptr = nullptr;
	if (in) {
		len = in->size();
		if (len > 0) {
			ptr = (uint8_t const *)in->data();
		}
	}
	dump(ptr, len);
}

bool misc::isText(const QString &mimetype)
{
	return mimetype.startsWith("text/");
}

bool misc::isSVG(const QString &mimetype)
{
	if (mimetype == "image/svg") return true;
	if (mimetype == "image/svg+xml") return true;
	return false;
}

bool misc::isPSD(const QString &mimetype)
{
	if (mimetype == "image/vnd.adobe.photoshop") return true;
	return false;
}

bool misc::isImage(const QString &mimetype)
{
#if 0
	if (mimetype == "image/jpeg") return true;
	if (mimetype == "image/jpg") return true;
	if (mimetype == "image/png") return true;
	if (mimetype == "image/gif") return true;
	if (mimetype == "image/bmp") return true;
	if (mimetype == "image/x-ms-bmp") return true;
	if (mimetype == "image/x-icon") return true;
	if (mimetype == "image/tiff") return true;
	if (isSVG(mimetype)) return true;
	if (isPSD(mimetype)) return true;
	return false;
#else
	return mimetype.startsWith("image/");
#endif
}

QString misc::abbrevBranchName(QString const &name)
{
	QStringList sl = name.split('/');
	if (sl.size() == 1) return sl[0];
	QString newname;
	for (int i = 0; i < sl.size(); i++) {
		QString s = sl[i];
		if (i + 1 < sl.size()) {
			s = s.mid(0, 1);
		}
		if (i > 0) {
			newname += '/';
		}
		newname += s;
	}
	return newname;
}

QString misc::determinFileType(const QString &filecommand, const QString &path, bool mime, std::function<void (const QString &, QByteArray *)> callback)
{ // ファイルタイプを調べる
	if (QFileInfo(filecommand).isExecutable()) {
		QString file = filecommand;
		QString mgc;
#ifdef Q_OS_WIN
		int i = file.lastIndexOf('/');
		int j = file.lastIndexOf('\\');
		if (i < j) i = j;
		if (i >= 0) {
			mgc = file.mid(0, i + 1) + "magic.mgc";
			if (QFileInfo(mgc).isReadable()) {
				// ok
			} else {
				mgc = QString();
			}
		}
#endif
		QString cmd;
		if (mgc.isEmpty()) {
			cmd = "\"%1\"";
			cmd = cmd.arg(file);
		} else {
			cmd = "\"%1\" -m \"%2\"";
			cmd = cmd.arg(file).arg(mgc);
			cmd = cmd.replace('\\', '/');
		}
		if (mime) {
			cmd += " --mime";
		}
		cmd += " --brief ";
		if (path == "-") {
			cmd += path;
		} else {
			cmd += QString("\"%1\"").arg(path);
		}

		// run file command

		QByteArray ba;

		callback(cmd, &ba);

		// parse file type

		if (!ba.isEmpty()) {
			QString s = QString::fromUtf8(ba).trimmed();
			QStringList list = s.split(';', QString::SkipEmptyParts);
			if (!list.isEmpty()) {
				QString mimetype = list[0].trimmed();
				return mimetype;
			}
		}

	} else {
		qDebug() << "No executable 'file' command";
	}
	return QString();
}

std::string misc::makeProxyServerURL(std::string text)
{
	if (!text.empty() && !strstr(text.c_str(), "://")) {
		text = "http://" + text;
		if (text[text.size() - 1] != '/') {
			text += '/';
		}
	}
	return text;
}

QString misc::makeProxyServerURL(QString text)
{
	if (!text.isEmpty() && text.indexOf("://") < 0) {
		text = "http://" + text;
		if (text[text.size() - 1] != '/') {
			text += '/';
		}
	}
	return text;
}

QPoint misc::contextMenuPos(QWidget *w, QContextMenuEvent *e)
{
	if (e && e->reason() == QContextMenuEvent::Mouse) {
		return QCursor::pos() + QPoint(8, -8);
	}
	return w->mapToGlobal(QPoint(4, 4));
}
