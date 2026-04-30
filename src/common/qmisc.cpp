#include "qmisc.h"
#include <QApplication>
#include <QStringList>
#include <QDateTime>
#include <QPainter>
#include <QWidget>
#include <QContextMenuEvent>

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
