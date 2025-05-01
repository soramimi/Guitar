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

/**
 * @brief アプリケーションのディレクトリパスを取得する
 * 
 * 実行可能ファイルが格納されているディレクトリのパスを返します。
 * Windowsの場合は'\\'、Unixの場合は'/'をパス区切り文字として扱います。
 * 
 * @return アプリケーションのディレクトリパス
 */
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
	QStringList ret;
	std::vector<std::string_view> lines = misc::splitLinesV(std::string_view(ba.data(), ba.size()), false);
	for (size_t i = 0; i < lines.size(); i++) {
		QString s = tos(lines[i].data(), lines[i].size());
		ret.push_back(s);
	}
	return ret;
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
 * @param str 分割する対象の文字列。
 * @param[out] out 分割された行を格納するstd::vectorへのポインタ。
 * @param keep_newline 改行文字を含めて行を格納する場合はtrue、そうでない場合はfalse。
 */
std::vector<std::string_view> misc::splitLinesV(std::string_view const &str, bool keep_newline)
{
	std::vector<std::string_view> ret;
	char const *begin = str.data();
	char const *end = begin + str.size();
	char const *ptr = begin;
	char const *left = ptr;
	while (1) {
		int c = 0;
		if (ptr < end) {
			c = (unsigned char)*ptr;
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
			ret.emplace_back(left, right - left);
			if (c == 0) break;
			left = ptr;
		} else {
			ptr++;
		}
	}
	return ret;
}

/**
 * @brief QByteArrayを行に分割する
 * 
 * QByteArrayの内容を文字列として行に分割し、std::string_viewのベクターとして返します。
 * 
 * @param ba 分割する対象のQByteArray
 * @param keep_newline 改行文字を含めて行を格納する場合はtrue、そうでない場合はfalse
 * @return 分割された行のリスト
 */
std::vector<std::string_view> misc::splitLinesV(QByteArray const &ba, bool keep_newline)
{
	return splitLinesV(std::string_view(ba.data(), ba.size()), keep_newline);
}

/**
 * @brief 文字列を行に分割する
 * 
 * 与えられた文字列を行に分割し、std::stringのベクターとして返します。
 * 
 * @param str 分割する対象の文字列
 * @param keep_newline 改行文字を含めて行を格納する場合はtrue、そうでない場合はfalse
 * @return 分割された行のリスト
 */
std::vector<std::string> misc::splitLines(std::string_view const &str, bool keep_newline)
{
	std::vector<std::string> out;
	std::vector<std::string_view> v = splitLinesV(str, keep_newline);
	for (auto const &s : v) {
		out.emplace_back(s);
	}
	return out;
}

/**
 * @brief 文字列を単語に分割する
 * 
 * 与えられた文字列を単語に分割し、std::string_viewのベクターとして返します。
 * 分割は、空白文字を区切りとして行われます。
 * 
 * @param text 分割する対象の文字列
 * @return 分割された単語のリスト
 */
std::vector<std::string_view> misc::splitWords(std::string_view const &text)
{
	std::vector<std::string_view> list;
	char const *begin = text.data();
	char const *end = begin + text.size();
	char const *ptr = begin;
	char const *left = ptr;
	while (1) {
		int c = 0;
		if (ptr < end) {
			c = (unsigned char)*ptr;
		}
		if (isspace(c) || c == 0) {
			if (left < ptr) {
				list.push_back(std::string_view(left, ptr - left));
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

/**
 * @brief パスからファイル名部分を取得する
 * 
 * 与えられたパスからファイル名部分のみを抽出します。
 * 
 * @param path ファイル名を抽出する対象のパス
 * @return 抽出されたファイル名
 */
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

/**
 * @brief 日時を文字列に変換する
 * 
 * 日時情報を読みやすい文字列形式に変換します。
 * デフォルトではISOフォーマットを使用し、'T'を空白に置換します。
 * 
 * @param dt 変換する日時情報
 * @return 変換された日時文字列。dt が無効な場合は空文字列
 */
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

/**
 * @brief 文字列が指定の文字列で始まるか判定する
 * 
 * 与えられた文字列が、指定された文字列で始まるかどうかを判定します。
 * 
 * @param str チェックする対象の文字列
 * @param with 先頭に存在するか確認する文字列
 * @return 先頭がwithで始まる場合はtrue、そうでない場合はfalse
 */
bool misc::starts_with(std::string const &str, std::string const &with)
{
	return strncmp(str.c_str(), with.c_str(), with.size()) == 0;
}

/**
 * @brief 文字列の一部分を取得する
 * 
 * 与えられた文字列の指定位置から、指定された長さの部分文字列を抽出します。
 * 
 * @param str 対象の文字列
 * @param start 抽出を開始する位置のインデックス
 * @param length 抽出する文字の数。負の場合は文字列の最後まで抽出する
 * @return 抽出された部分文字列
 */
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

/**
 * @brief パスの区切り文字を正規化する
 * 
 * パスの区切り文字をプラットフォームに合わせて正規化します。
 * Windowsでは '/' を '\\' に変換し、
 * Unix系プラットフォームでは変換を行いません。
 * 
 * @param str 正規化するパス文字列
 * @return 正規化されたパス文字列
 */
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

/**
 * @brief 2つのパスをスラッシュで結合する
 * 
 * 2つのパス文字列を適切な区切り文字を使用して結合します。
 * 空の文字列が渡された場合は空でない方を返します。
 * 
 * @param left 左側のパス文字列
 * @param right 右側のパス文字列
 * @return 結合されたパス文字列
 */
QString misc::joinWithSlash(QString const &left, QString const &right)
{
	if (!left.isEmpty() && !right.isEmpty()) {
		return joinpath(left, right);
	}
	return !left.isEmpty() ? left : right;
}

/**
 * @brief ウィジェットのサイズを固定する
 * 
 * ウィジェットのサイズを固定し、リサイズできないようにします。
 * また、コンテキストヘルプボタンを非表示にし、Windowsの固定サイズダイアログヒントを設定します。
 * 
 * @param w サイズを固定する対象のウィジェット
 */
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
 * @brief メモリダンプを16進数で表示する
 *
 * 与えられたメモリ領域を16進数でダンプし、表示します。ダンプは、
 * アドレス、16進数データ（最大16バイトずつ）、ASCII文字列表現の3つの列で構成されます。
 * 表示されない制御文字はピリオド（.）で表示されます。
 *
 * @param ptr ダンプするメモリ領域の先頭ポインタ、nullptrの場合は何も表示しません
 * @param len ダンプするメモリ領域の長さ（バイト数）
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

/**
 * @brief バイナリデータの内容をヘキサでダンプする
 * 
 * QByteArrayの内容をヘキサダンプ形式で表示します。
 * 内部的にはdump(uint8_t const *, size_t)関数を使用して、
 * QByteArrayの内容をバイト配列としてダンプします。
 * 
 * @param in ダンプするQByteArrayへのポインタ、nullptrの場合は何も表示しません
 */
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

/**
 * @brief MIMEタイプがテキストファイルを表すか判定する
 * 
 * 与えられたMIMEタイプがテキストファイルを表すか判定します。
 * 
 * @param mimetype 判定するMIMEタイプ文字列
 * @return テキストファイルを表す場合はtrue、そうでない場合はfalse
 */
bool misc::isText(QString const &mimetype)
{
	return mimetype.startsWith("text/");
}

/**
 * @brief MIMEタイプがSVG画像を表すか判定する
 * 
 * 与えられたMIMEタイプがSVG画像ファイルを表すか判定します。
 * 
 * @param mimetype 判定するMIMEタイプ文字列
 * @return SVG画像を表す場合はtrue、そうでない場合はfalse
 */
bool misc::isSVG(QString const &mimetype)
{
	if (mimetype == "image/svg") return true;
	if (mimetype == "image/svg+xml") return true;
	return false;
}

/**
 * @brief MIMEタイプがPhotoshopファイルを表すか判定する
 * 
 * 与えられたMIMEタイプがAdobe Photoshopのファイルを表すか判定します。
 * 
 * @param mimetype 判定するMIMEタイプ文字列
 * @return Photoshopファイルを表す場合はtrue、そうでない場合はfalse
 */
bool misc::isPSD(QString const &mimetype)
{
	if (mimetype == "image/vnd.adobe.photoshop") return true;
	return false;
}

/**
 * @brief MIMEタイプがPDFファイルを表すか判定する
 * 
 * 与えられたMIMEタイプがPDFファイルを表すか判定します。
 * 
 * @param mimetype 判定するMIMEタイプ文字列
 * @return PDFファイルを表す場合はtrue、そうでない場合はfalse
 */
bool misc::isPDF(QString const &mimetype)
{
	if (mimetype == "application/pdf") return true;
	return false;
}

/**
 * @brief MIMEタイプが画像ファイルを表すか判定する
 * 
 * 与えられたMIMEタイプが画像ファイルを表すか判定します。
 * PDFファイルや「image/」で始まる全てのMIMEタイプを画像として扱います。
 * 
 * @param mimetype 判定するMIMEタイプ文字列
 * @return 画像ファイルを表す場合はtrue、そうでない場合はfalse
 */
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
	if (isPDF(mimetype)) return true;
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

/**
 * @brief プロキシサーバーURLを正規化する
 * 
 * 入力されたプロキシサーバーのURLを正規化します。
 * プロトコルが指定されていない場合は "http://" を付加し、
 * 末尾にスラッシュがない場合は付加します。
 * 
 * @param text 正規化するプロキシサーバーURL文字列
 * @return 正規化されたプロキシサーバーURL
 */
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

/**
 * @brief プロキシサーバーURLを正規化する
 * 
 * 入力されたプロキシサーバーのURLを正規化します。
 * プロトコルが指定されていない場合は "http://" を付加し、
 * 末尾にスラッシュがない場合は付加します。
 * 
 * @param text 正規化するプロキシサーバーURL文字列
 * @return 正規化されたプロキシサーバーURL
 */
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

/**
 * @brief コンテキストメニューを表示する位置を計算する
 * 
 * コンテキストメニューイベントに基づいて、メニューを表示する適切な位置を計算します。
 * マウスによる右クリックの場合はカーソルの位置にオフセット(8,-8)を加え、
 * それ以外の場合（キーボードでの表示等）はウィジェットの左上から少し離れた位置(4,4)を返します。
 * 
 * @param w コンテキストメニューを表示するウィジェット
 * @param e コンテキストメニューイベント
 * @return メニューを表示するグローバル座標位置
 */
QPoint misc::contextMenuPos(QWidget *w, QContextMenuEvent *e)
{
	if (e && e->reason() == QContextMenuEvent::Mouse) {
		return QCursor::pos() + QPoint(8, -8);
	}
	return w->mapToGlobal(QPoint(4, 4));
}

/**
 * @brief ファイルが実行可能か判定する
 * 
 * 指定されたファイルパスが実行可能なファイルを指しているか判定します。
 * 
 * @param cmd 判定するファイルパス
 * @return 実行可能な場合はtrue、そうでない場合はfalse
 */
bool misc::isExecutable(QString const &cmd)
{
	QFileInfo info(cmd);
	return info.isExecutable();
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
 * 
 * 文字列が有効なメールアドレス形式かどうかを判定します。
 * 単純に@記号が含まれており、先頭でも末尾でもないことを確認します。
 * 
 * @param email 検証するメールアドレス文字列
 * @return 有効なメールアドレス形式の場合はtrue、そうでない場合はfalse
 */
bool misc::isValidMailAddress(const QString &email)
{
	int i = email.indexOf('@');
	return i > 0 && i < email.size() - 1;
}

/**
 * @brief 文字列が有効なメールアドレスか判定する
 * 
 * std::string形式の文字列が有効なメールアドレス形式かどうかを判定します。
 * 内部的にはQString版のisValidMailAddressに変換して処理します。
 * 
 * @param email 検証するメールアドレス文字列
 * @return 有効なメールアドレス形式の場合はtrue、そうでない場合はfalse
 */
bool misc::isValidMailAddress(const std::string &email)
{
	return isValidMailAddress(QString::fromStdString(email));
}

/**
 * @brief 文字列の両端から空白文字を取り除く
 * 
 * 文字列の先頭と末尾から空白文字（スペース、タブ、改行など）を削除します。
 * 元の文字列は変更せず、新しいstring_viewを返します。
 * 
 * @param s トリムする文字列
 * @return 両端の空白が削除された文字列ビュー
 */
std::string_view misc::trimmed(const std::string_view &s)
{
	size_t i = 0;
	size_t j = s.size();
	while (i < j && std::isspace((unsigned char)s[i])) i++;
	while (i < j && std::isspace((unsigned char)s[j - 1])) j--;
	return s.substr(i, j - i);
}

/**
 * @brief 文字列の両端から空白文字と引用符を取り除く
 * 
 * 文字列の先頭と末尾から空白文字を削除し、その後引用符（'"'または'\'')で囲まれている場合は
 * それらの引用符も削除します。元の文字列は変更せず、新しいstring_viewを返します。
 * 
 * @param s トリムする文字列
 * @return 両端の空白と引用符が削除された文字列ビュー
 */
std::string_view misc::trimQuotes(std::string_view s)
{
	s = trimmed(s);
	if (s.size() >= 2) {
		if (s[0] == '"' && s[s.size() - 1] == '"') {
			s = s.substr(1, s.size() - 2);
		} else if (s[0] == '\'' && s[s.size() - 1] == '\'') {
			s = s.substr(1, s.size() - 2);
		}
	}
	return s;
}

/**
 * @brief 文字列の両端から改行文字を取り除く
 * 
 * 文字列の先頭と末尾から改行文字（CR、LF、CRLF）を削除します。
 * 元の文字列は変更せず、新しいstring_viewを返します。
 * CR+LFの組み合わせは1つの改行として扱われます。
 * 
 * @param s トリムする文字列
 * @return 両端の改行が削除された文字列ビュー
 */
std::string_view misc::trimNewLines(std::string_view s)
{
	size_t i = 0;;
	size_t j = s.size();
	if (i < j) {
		if (s[i] == '\r') {
			i++;
			if (i < j && s[i] == '\n') {
				i++;
			}
		} else if (s[i] == '\n') {
			i++;
		}
	}
	if (i < j) {
		if (s[j - 1] == '\n') {
			j--;
			if (j > i && s[j - 1] == '\r') {
				j--;
			}
		} else if (s[j - 1] == '\r') {
			j--;
		}
	}
	return s.substr(i, j - i);
}


/**
 * @brief バイナリデータを16進数文字列に変換する
 * 
 * バイナリデータ（バイト列）を16進数表記の文字列に変換します。
 * 各バイトは2桁の16進数で表現されます（00-FF）。
 * 
 * @param begin 変換するバイナリデータの先頭ポインタ
 * @param end 変換するバイナリデータの終端ポインタ（変換対象に含まれない）
 * @return 16進数文字列
 */
std::string misc::bin_to_hex_string(const void *begin, const void *end)
{
	std::vector<char> vec;
	uint8_t const *p = (uint8_t const *)begin;
	uint8_t const *e = (uint8_t const *)end;
	vec.reserve((e - p) * 2);
	while (p < e) {
		char tmp[3];
		sprintf(tmp, "%02x", *p);
		vec.push_back(tmp[0]);
		vec.push_back(tmp[1]);
		p++;
	}
	return std::string(&vec[0], vec.size());
}

/**
 * @brief 16進数文字列をバイナリデータに変換する
 * 
 * 16進数表記の文字列をバイナリデータ（バイト列）に変換します。
 * 文字列には2桁の16進数表記（00-FF）を含める必要があります。
 * オプションで区切り文字を指定することもできます。
 * 
 * @param s 変換する16進数文字列
 * @param sep 区切り文字の文字列。区切り文字はスキップされる。nullptrの場合は区切り文字なし
 * @return 変換されたバイナリデータ
 */
std::vector<uint8_t> misc::hex_string_to_bin(const std::string_view &s, char const *sep)
{
	std::vector<uint8_t> vec;
	vec.reserve(s.size() / 2);
	unsigned char const *begin = (unsigned char const *)s.data();
	unsigned char const *end = begin + s.size();
	unsigned char const *p = begin;
	while (p + 1 < end) {
		if (isxdigit(p[0]) && isxdigit(p[1])) {
			char tmp[3];
			tmp[0] = p[0];
			tmp[1] = p[1];
			tmp[2] = 0;
			uint8_t c = (uint8_t)strtol(tmp, nullptr, 16);
			vec.push_back(c);
			p += 2;
		} else if (sep && strchr(sep, *p)) {
			p++;
		} else {
			break;
		}
	}
	return vec;
}

/**
 * @brief 2つのバイナリデータを比較する
 * 
 * 2つのバイト配列を辞書的に比較します。最初に異なるバイトが見つかった時点で
 * その大小関係を返します。同じバイト列で長さが異なる場合は、長い方が大きいと判定されます。
 * 
 * @param a 比較する最初のバイト配列
 * @param n 最初の配列の長さ
 * @param b 比較する2番目のバイト配列
 * @param m 2番目の配列の長さ
 * @return a < b の場合は-1、a > b の場合は1、a == b の場合は0
 */
int misc::compare(uint8_t const *a, size_t n, uint8_t const *b, size_t m)
{
	size_t i = 0;
	while (1) {
		if (i < n && i < m) {
			uint8_t c = a[i];
			uint8_t d = b[i];
			if (c < d) return -1;
			if (c > d) return 1;
			i++;
		} else if (i < n) {
			return 1;
		} else if (i < m) {
			return -1;
		} else {
			return 0;
		}
	}
	return 0;
}

/**
 * @brief 2つのバイトベクターを比較する
 * 
 * 2つのバイトベクターを辞書的に比較します。
 * 内部的にはcompare関数を使用して、ベクターの内容と長さを比較します。
 * 
 * @param a 比較する最初のバイトベクター
 * @param b 比較する2番目のバイトベクター
 * @return a < b の場合は-1、a > b の場合は1、a == b の場合は0
 */
int misc::compare(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
{
	return compare(a.data(), a.size(), b.data(), b.size());
}

