#include "common/misc.h"
#include <QDebug>
#include <QFileInfo>
#include <QPainter>
#include <QProcess>
#include <QWidget>
#include <QContextMenuEvent>
#include <vector>
#include "common/joinpath.h"
#include "misc.h"

QString misc::getApplicationDir()
{
	QString path = QApplication::applicationFilePath();
	int i = path.lastIndexOf('\\');
	int j = path.lastIndexOf('/');
	if (i < j) i = j;
	if (i > 0) path = path.mid(0, i);
	return path;
}

/**
 * @brief QByteArrayの文字列を行に分割する。
 *
 * 与えられたQByteArrayの文字列を行に分割し、QStringListとして返します。
 * 分割は、改行文字 ('\n' または '\r\n') を区切りとして行われます。
 * また、与えられた変換関数を使用して、charの配列をQStringに変換します。
 *
 * @param ba 分割する対象のQByteArray。
 * @param tos 文字列を変換する関数。charの配列とその長さを引数に取り、QStringを返す関数。
 * @return 分割された行のリスト。
 */
QStringList misc::splitLines(QByteArray const &ba, std::function<QString(char const *ptr, size_t len)> const &tos)
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

/**
 * @brief 文字列を行に分割する。
 *
 * 与えられた文字列を行に分割し、QStringListとして返します。
 * 分割は、改行文字 ('\n' または '\r\n') を区切りとして行われます。
 *
 * @param text 分割する対象の文字列。
 * @return 分割された行のリスト。
 */
QStringList misc::splitLines(QString const &text)
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
			list.push_back(QString::fromUtf16((char16_t const *)left, int(ptr - left)));
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

/**
 * @brief 文字列を行に分割する。
 *
 * 与えられた文字列を行に分割し、std::vector<std::string>として返します。
 * 分割は、改行文字 ('\n' または '\r\n') を区切りとして行われます。
 *
 * @param begin 分割する対象の文字列の先頭ポインタ。
 * @param end 分割する対象の文字列の終端ポインタ。
 * @param[out] out 分割された行を格納するstd::vectorへのポインタ。
 * @param keep_newline 改行文字を含めて行を格納する場合はtrue、そうでない場合はfalse。
 */
void misc::splitLines(char const *begin, char const *end, std::vector<std::string> *out, bool keep_newline)
{
	char const *ptr = begin;
	char const *left = ptr;
	while (1) {
		char c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (c == '\n' || c == '\r' || c == 0) {
			char const *right = ptr;
			if (c == '\n') {
				ptr++;
			} else if (c == '\r') {
				ptr++;
				if (ptr < end && *ptr == '\n') {
					ptr++;
				}
			}
			if (keep_newline) {
				right = ptr;
			}
			out->push_back(std::string(left, right - left));
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

/**
 * @brief 文字列を単語に分割する。
 *
 * 与えられた文字列を単語に分割し、QStringListとして返します。分割は、空白文字を
 * 区切りとして行われます。
 *
 * @param text 分割する対象の文字列。
 * @return 分割された単語のリスト。
 */
QStringList misc::splitWords(QString const &text)
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
				list.push_back(QString::fromUtf16((char16_t const *)left, int(ptr - left)));
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
#if 0
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
#elif 0
		QString s = dt.toLocalTime().toString(Qt::DefaultLocaleShortDate);
		return s;
#else
		QString s = dt.toLocalTime().toString(Qt::ISODate);
		s.replace('T', ' ');
		return s;
#endif
	}
	return QString();
}

bool misc::starts_with(std::string const &str, std::string const &with)
{
	return strncmp(str.c_str(), with.c_str(), with.size()) == 0;
}

std::string misc::mid(std::string const &str, int start, int length)
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

void misc::setFixedSize(QWidget *w)
{
	Qt::WindowFlags flags = w->windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags |= Qt::MSWindowsFixedSizeDialogHint;
	w->setWindowFlags(flags);
	w->setFixedSize(w->size());
}

/**
 * @brief 矩形のフレームを描画する。
 *
 * 指定された位置とサイズの矩形のフレームを描画します。フレームの上辺と左辺は、
 * color_topleftで指定された色で描画され、右辺と下辺はcolor_bottomrightで指定された色で描画されます。
 *
 * @param pr フレームを描画する対象のQPainterオブジェクトへのポインタ。
 * @param x 矩形の左上隅のX座標。
 * @param y 矩形の左上隅のY座標。
 * @param w 矩形の幅。
 * @param h 矩形の高さ。
 * @param color_topleft フレームの上辺と左辺の色。
 * @param color_bottomright フレームの右辺と下辺の色。
 */
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

/**
 * @brief メモリダンプを16進数で表示する。
 *
 * 与えられたメモリ領域を16進数でダンプし、表示します。ダンプは、
 * アドレス、16進数データ、ASCII文字列の3つの列で構成されます。
 *
 * @param ptr ダンプするメモリ領域の先頭ポインタ。
 * @param len ダンプするメモリ領域の長さ。
 */
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
                *dst = (char)c;
				dst++;
			}
			*dst = 0;
		}
	}
}

void misc::dump(QByteArray const *in)
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

bool misc::isText(QString const &mimetype)
{
	return mimetype.startsWith("text/");
}

bool misc::isSVG(QString const &mimetype)
{
	if (mimetype == "image/svg") return true;
	if (mimetype == "image/svg+xml") return true;
	return false;
}

bool misc::isPSD(QString const &mimetype)
{
	if (mimetype == "image/vnd.adobe.photoshop") return true;
	return false;
}

bool misc::isImage(QString const &mimetype)
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

/**
 * @brief ブランチ名を短縮形に変換する。
 *
 * 入力されたブランチ名を短縮形に変換します。ブランチ名の各パス要素の先頭文字を抽出し、
 * 最後のパス要素を除いて短縮形にします。
 *
 * @param name 短縮形に変換する対象のブランチ名。
 * @return 短縮されたブランチ名。
 */
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

bool misc::isExecutable(QString const &cmd)
{
	QFileInfo info(cmd);
	return info.isExecutable();
}

/**
 * @brief リモートURLの形式を補完または変換する。
 *
 * 入力されたリモートURLの形式を補完または変換します。
 * 以下の3つのパターンが対象です。
 * 1. "https://github.com/" で始まるURLを "git@github.com:" 形式に変換
 * 2. "git@github.com:" で始まるURLを "https://github.com/" 形式に変換
 * 3. "github" で始まるURLを "https://github.com/" 形式に補完
 *
 * @param url 変換または補完対象のリモートURL。
 * @param toggle 変換オプション。trueの場合、変換を実行。falseの場合、補完のみ実行。
 * @return 変換または補完されたリモートURL。
 */
QString misc::complementRemoteURL(QString url, bool toggle)
{
	if (toggle && url.startsWith("https://github.com/")) {
		url = "git@github.com:" + url.mid(19);
	} else if (toggle && url.startsWith("git@github.com:")) {
		url = "https://github.com/" + url.mid(15);
	} else {
		QStringList s = misc::splitWords(url);
		if (s.size() == 3 && s[0] == "github") {
			QString name = s[2];
			if (name.endsWith(".git")) {
				name = name.mid(0, name.size() - 4);
			}
			url = "https://github.com/" + s[1] + '/' + name + ".git";
		}
	}
	return url;
}

/**
 * @brief 文字列内の連続する空白文字を1つのスペースにまとめる。
 *
 * 入力された文字列内の連続する空白文字を1つのスペースにまとめ、結果の文字列を返します。
 *
 * @param source 連続する空白文字をまとめる対象のQStringオブジェクト。
 * @return 連続する空白文字が1つのスペースにまとめられたQStringオブジェクト。
 */
QString misc::collapseWhitespace(const QString &source)
{
	QChar *p = (QChar *)alloca(sizeof(QChar) * (source.size() + 1));
	QChar const *r = source.unicode();
	QChar *w = p;
	bool spc = false;
	while (1) {
		QChar c = *r;
		if (c > ' ') {
			if (spc) {
				*w++ = ' ';
				spc = false;
			}
			*w++ = c;
		} else {
			if (c == 0) {
				*w = QChar::Null;
				break;
			}
			spc = true;
		}
		r++;
	}
	return QString(p);
}

/**
 * @brief 文字列が有効なメールアドレスか判定する
 * @param email
 * @return
 */
bool misc::isValidMailAddress(const QString &email)
{
	int i = email.indexOf('@');
	return i > 0 && i < email.size() - 1;
}

bool misc::isValidMailAddress(const std::string &email)
{
	return isValidMailAddress(QString::fromStdString(email));
}
