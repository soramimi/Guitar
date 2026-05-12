#include "qmisc.h"
#include <QApplication>
#include <QStringList>
#include <QDateTime>
#include <QPainter>
#include <QWidget>
#include <QContextMenuEvent>

/**
 * @brief 文字列を単語に分割する。
 *
 * 与えられた文字列を単語に分割し、QStringListとして返します。分割は、空白文字を
 * 区切りとして行われます。
 *
 * @param text 分割する対象の文字列。
 * @return 分割された単語のリスト。
 */
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

