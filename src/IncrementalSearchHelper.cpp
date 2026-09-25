#include "IncrementalSearchHelper.h"
#include <QColor>
#include <QPainter>
#include <QStyleOptionViewItem>

#ifdef APP_GUITAR
#include "ApplicationGlobal.h"
#endif

namespace incrementalsearch {

#ifdef APP_GUITAR
QColor filtered_bg_color()
{
	return global->appsettings.incremental_search_color.filtered_bg;
}

QColor highlight_bg_color()
{
	return global->appsettings.incremental_search_color.highlight_bg;
}
#else
QColor filtered_bg_color()
{
	return QColor(128, 128, 128, 64);
}

QColor highlight_bg_color()
{
	return QColor(240, 64, 255, 128);
}
#endif

QString normalizeText(QString s)
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

void drawText(QPainter *painter, const QStyleOptionViewItem &opt, QRect r, const QString &text)
{
	painter->setPen(opt.palette.color(QPalette::Text));
	painter->drawText(r, opt.displayAlignment, text); // テキストを描画
}

void drawText_filtered(QPainter *painter, const QStyleOptionViewItem &opt, const QRect &rect, IncrementalSearchFilter const &filter)
{
	if (!filter) {
		drawText(painter, opt, rect, opt.text);
		return;
	}

	QString text = opt.text;

	Result match = global->incremental_search->match(text.toStdString(), filter);
	if (match) {
		int x = rect.x();
		for (Result::Part const &part : match.parts) {
			QString s = QString::fromStdString(part.text);
			int w = painter->fontMetrics().horizontalAdvance(s);
			QRect r = rect;
			r.setLeft(x);
			r.setWidth(w);
			if (part.match) { // フィルターの部分の背景をハイライト
				painter->fillRect(r, incrementalsearch::highlight_bg_color());
			}
			drawText(painter, opt, r, s);
			x += w;
		}
	} else {
		drawText(painter, opt, rect, text);
	}
}

void fillFilteredBG(QPainter *painter, const QRect &rect)
{
	painter->fillRect(rect, incrementalsearch::filtered_bg_color());
}

QString appendCharToFilterText(QString filter, const QString &add)
{
	uchar c = *add.utf16();
	if (c == ASCII_BACKSPACE) {
		int i = filter.size();
		if (i > 0) {
			filter.remove(i - 1, 1);
		}
	} else if (c == ASCII_DELETE) {
		filter.clear();
	} else if (c >= 0x20 && c < 0x80 && (!isspace(c) && isprint(c))) {
		filter.append(QChar(c));
	}	
	return filter;
}

} // namespace incrementalsearch
