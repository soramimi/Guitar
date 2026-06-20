#include "IncrementalSearchHelper.h"
#include <QColor>
#include <QPainter>
#include <QStyleOptionViewItem>
#include "ApplicationGlobal.h"

static inline QColor incremental_search_filtered_bg_color()
{
	return global->appsettings.incremental_search_color.filtered_bg;
}

static inline QColor incremental_search_highlight_bg_color()
{
	return global->appsettings.incremental_search_color.highlight_bg;
}

// incrementalsearch

QString incrementalsearch::normalizeText(QString s)
{
	for (QChar &c : s) {
		if (c >= 'A' && c <= 'Z') { // 大文字を小文字に
			c = QChar(c.unicode() - 'A' + 'a');
		} else if (c >= QChar(0x3041) && c <= QChar(0x3096)) { // ひらがなをカタカナに
			c = QChar(c.unicode() + 0x60);
		} else if (c >= QChar(0xFF61) && c <= QChar(0xFF9F)) { // 半角カナを全角カナに
			c = QChar(c.unicode() - 0xFF61 + 0x30A0);
		} else if (c >= QChar(0xFF21) && c <= QChar(0xFF3A)) { // 全角英大文字を半角英小文字に
			c = QChar(c.unicode() - 0xFF21 + 'a');
		} else if (c >= QChar(0xFF41) && c <= QChar(0xFF5A)) { // 全角英小文字を半角英小文字に
			c = QChar(c.unicode() - 0xFF41 + 'a');
		}
	}
	return s;
}

void incrementalsearch::drawText(QPainter *painter, const QStyleOptionViewItem &opt, QRect r, const QString &text)
{
	painter->setPen(opt.palette.color(QPalette::Text));
	painter->drawText(r, opt.displayAlignment, text); // テキストを描画
}

void incrementalsearch::drawText_filtered(QPainter *painter, const QStyleOptionViewItem &opt, const QRect &rect, IncrementalSearchFilter const &filter)
{
	if (!filter) {
		drawText(painter, opt, rect, opt.text);
		return;
	}

	QString text = opt.text;

	incrementalsearch::Result match = global->incremental_search->match(text.toStdString(), filter);
	if (match) {
		int x = rect.x();
		for (incrementalsearch::Result::Part const &part : match.parts) {
			QString s = QString::fromStdString(part.text);
			int w = painter->fontMetrics().size(Qt::TextSingleLine, s).width();
			QRect r = rect;
			r.setLeft(x);
			r.setWidth(w);
			if (part.match) { // フィルターの部分の背景をハイライト
				painter->fillRect(r, incremental_search_highlight_bg_color());
			}
			drawText(painter, opt, r, s);
			x += w;
		}
	} else {
		drawText(painter, opt, rect, text);
	}
}

void incrementalsearch::fillFilteredBG(QPainter *painter, const QRect &rect)
{
	painter->fillRect(rect, incremental_search_filtered_bg_color());
}

