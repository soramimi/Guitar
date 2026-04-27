#include "IncrementalSearch.h"
#include "ApplicationGlobal.h"
#include "LibMigemo.h"
#include "common/joinpath.h"
#include "zip/zip.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QPainter>
#include <QRegularExpression>
#include <QStyleOptionViewItem>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>


MigemoFilter::MigemoFilter(std::string const &filtertext)
{
	makeFilter(filtertext);
}

bool MigemoFilter::isEmpty() const
{
	return text_.empty();
}

void MigemoFilter::makeFilter(std::string const &filtertext)
{
	text_ = filtertext;
	if (LibMigemo::instance()->migemoEnabled()) {
		if (filtertext.size() > 0) {
			auto s = LibMigemo::instance()->queryMigemo(filtertext.c_str());
			if (s) {
				re_ = std::make_shared<QRegularExpression>(QString::fromStdString(*s), QRegularExpression::CaseInsensitiveOption);
			}
		}
	}
}

IncrementalSearch::Result MigemoFilter::match(std::string const &text) const
{
	using namespace IncrementalSearch;

	if (isEmpty()) {
		Result ret;
		ret.match = true; // フィルターが空の場合は常にtrue
		return ret;
	}
	if (re_.get()) { // 正規表現が有効な場合
		QString text1 = QString::fromStdString(text);
		QString text2 = IncrementalSearch::normalizeText(text1);
		QRegularExpressionMatch m;
		if (text2.contains(*re_, &m)) {
			Result ret;
			ret.match = true;
			ret.pos = m.capturedStart();
			ret.end = m.capturedEnd();
			{
				Part part;
				part.source.pos = 0;
				part.source.end = ret.pos;
				part.source.text = text1.mid(0, ret.pos).toStdString();
				part.match = false;
				ret.part.push_back(part);
			}
			{
				Part part;
				part.source.pos = ret.pos;
				part.source.end = ret.end;
				part.source.text = text1.mid(ret.pos, ret.end - ret.pos).toStdString();
				part.match = true;
				ret.part.push_back(part);
			}
			{
				Part part;
				part.source.pos = ret.end;
				part.source.end = text.size();
				part.source.text = text1.mid(ret.end).toStdString();
				part.match = false;
				ret.part.push_back(part);
			}
			return ret;
		}
	}
	return {};
}

// IncrementalSearch

QString IncrementalSearch::normalizeText(QString s)
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

void IncrementalSearch::drawText(QPainter *painter, const QStyleOptionViewItem &opt, QRect r, const QString &text)
{
#ifndef Q_OS_WIN
	if (opt.state & QStyle::State_Selected) { // 選択されている場合
		painter->setPen(opt.palette.color(QPalette::HighlightedText));
	} else {
		painter->setPen(opt.palette.color(QPalette::Text));
	}
#endif
	painter->drawText(r, opt.displayAlignment, text); // テキストを描画
}

void IncrementalSearch::drawText_filtered(QPainter *painter, const QStyleOptionViewItem &opt, const QRect &rect, IncrementalSearchFilter const &filter)
{
	if (!filter) {
		drawText(painter, opt, rect, opt.text);
		return;
	}

	QString text = opt.text;

	std::vector<std::tuple<QString, bool>> list;

	IncrementalSearch::Result match = IncrementalSearch::match(text.toStdString(), filter);
	if (match) {
		for (IncrementalSearch::Part const &Part : match.part) {
			list.push_back(std::make_tuple(QString::fromStdString(Part.source.text), Part.match));
		}

		int x = rect.x();
		for (const auto &[s, f] : list) {
			int w = painter->fontMetrics().size(Qt::TextSingleLine, s).width();
			QRect rect2 = rect;
			rect2.setLeft(x);
			rect2.setWidth(w);
			if (f) { // フィルターの部分をハイライト
				QColor color = opt.palette.color(QPalette::Highlight);
				color.setAlpha(128);
				painter->fillRect(rect2, color);
			}
			drawText(painter, opt, rect2, s);
			x += w;
		}
	} else {
		drawText(painter, opt, rect, text);
	}
}

void IncrementalSearch::fillFilteredBG(QPainter *painter, const QRect &rect)
{
	painter->fillRect(rect, QColor(128, 128, 128, 64));
}

// MecabFilter

std::string MecabFilter::to_kana(const std::string &text, std::vector<IncrementalSearch::Part> *out)
{
	std::string kana;
	std::vector<LibMecab::Part> parts = global->mecab.parse(text);
	size_t pos = 0;
	for (LibMecab::Part const &part : parts) {
		IncrementalSearch::Part Part;
		Part.source.text = text.substr(part.offset, part.length);
		Part.source.pos = kana.size();
		Part.source.end = part.offset + part.length;
		auto end = pos + part.text.size();
		Part.kana.text = part.text;
		Part.kana.pos = pos;
		Part.kana.end = end;
		pos = end;
		out->push_back(Part);
		kana.append(part.text);
	}
	return kana;
}

MecabFilter::MecabFilter(std::string const &filtertext)
{
	makeFilter(filtertext);
}

void MecabFilter::makeFilter(std::string const &filtertext)
{
	original_text_ = filtertext;
	katakana_text_ = global->mecab.convert_roman_to_katakana(original_text_);
}

bool MecabFilter::isEmpty() const
{
	return original_text_.empty();
}

IncrementalSearch::Result MecabFilter::match(std::string const &text) const
{
	using namespace IncrementalSearch;

	Result ret;
	{
		char const *p = misc::stristr(text.c_str(), original_text_.c_str());
		if (p) {
			size_t pos = p - text.c_str();
			ret.match = true;
			ret.pos = pos;
			ret.end = pos + original_text_.size();
			if (ret.pos < ret.end) {
				{
					Part part;
					part.source.pos = 0;
					part.source.end = ret.pos;
					part.source.text = text.substr(0, ret.pos);
					part.match = false;
					ret.part.push_back(part);
				}
				{
					Part part;
					part.source.pos = ret.pos;
					part.source.end = ret.end;
					part.source.text = text.substr(ret.pos, ret.end - ret.pos);
					part.match = true;
					ret.part.push_back(part);
				}
				{
					Part part;
					part.source.pos = ret.end;
					part.source.end = text.size();
					part.source.text = text.substr(ret.end);
					part.match = false;
					ret.part.push_back(part);
				}
				return ret;
			}
		}
	}
	if (!katakana_text_.empty()) {
		std::vector<Part> Part;
		std::string kana = to_kana(text, &Part);
		char const *p = strstr(kana.c_str(), katakana_text_.c_str());
		if (p) {
			size_t pos = p - kana.c_str();
			size_t end = pos + katakana_text_.size();
			ret.match = true;
			ret.pos = pos;
			for (size_t i = 0; i < Part.size(); i++) {
				if (pos <= Part[i].kana.pos && Part[i].kana.pos < end) {
					ret.end = Part[i].kana.end;
					Part[i].match = true;
				}
			}
		}
		ret.part = Part;
	}
	return ret;
}
