#include "IncrementalSearch.h"
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
	painter->setPen(opt.palette.color(QPalette::Text));
	painter->drawText(r, opt.displayAlignment, text); // テキストを描画
}

void IncrementalSearch::drawText_filtered(QPainter *painter, const QStyleOptionViewItem &opt, const QRect &rect, IncrementalSearchFilter const &filter)
{
	if (!filter) {
		drawText(painter, opt, rect, opt.text);
		return;
	}

	QString text = opt.text;

	IncrementalSearch::Result match = IncrementalSearch::match(text.toStdString(), filter);
	if (match) {
		int x = rect.x();
		for (IncrementalSearch::ResultPart const &part : match.parts) {
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

void IncrementalSearch::fillFilteredBG(QPainter *painter, const QRect &rect)
{
	painter->fillRect(rect, incremental_search_filtered_bg_color());
}

// MecabFilter

struct MecabFilter::Private {
	std::string original_text;
	std::string katakana_text;
};

std::string MecabFilter::to_kana(const std::string &text, std::vector<IncrementalSearch::Part> *out)
{
	std::string kana;
#ifdef USE_EXPERIMENTAL_JAGGER
	std::vector<LibMecab::Part> parts = global->jagger.parse(text);
#else
	std::vector<LibMecab::Part> parts = global->mecab.parse(text);
#endif
	size_t pos = 0;
	for (LibMecab::Part const &part : parts) {
		IncrementalSearch::Part item;
		item.source.text = text.substr(part.offset, part.length);
		item.source.pos = part.offset;
		item.source.end = part.offset + part.length;
		auto end = pos + part.text.size();
		item.kana.text = part.text;
		item.kana.pos = pos;
		item.kana.end = end;
		pos = end;
		if (out) {
			out->push_back(item);
		}
		kana.append(part.text);
	}
	return kana;
}

MecabFilter::MecabFilter()
	: m(new Private())
{
}

MecabFilter::MecabFilter(std::string const &filtertext)
	: m(new Private())
{
	makeFilter(filtertext);
}

MecabFilter::~MecabFilter()
{
	delete m;
}

void MecabFilter::makeFilter(std::string const &filtertext)
{
	m->original_text = filtertext;
	m->katakana_text = global->mecab.convert_roman_to_katakana(m->original_text);
}

bool MecabFilter::isEmpty() const
{
	return m->original_text.empty();
}

IncrementalSearch::Result MecabFilter::match(std::string const &text) const
{
	using namespace IncrementalSearch;
	Result ret;
	if (m->original_text.empty()) {
		// フィルタ文字列なしは全体一致とみなす
		ResultPart part;
		part.match = false;
		part.pos = 0;
		part.end = text.size();
		part.text = text;
		ret.parts.push_back(part);
		ret.match = true;
	} else {
		std::vector<ResultPart> matched_list;
		// フィルタ文字列をそのままマッチさせる
		char const *ptr = text.c_str();
		while (ptr) {
			char const *match = misc::stristr(ptr, m->original_text.c_str());
			if (!match) break;
			// マッチした部分を追加
			Part part;
			part.match = true;
			part.source.pos = match - text.c_str();
			part.source.end = part.source.pos + m->original_text.size();
			part.source.text = text.substr(part.source.pos, m->original_text.size());
			matched_list.push_back(part);
			ptr = match + m->original_text.size();
		}
		// フィルタ文字列をカタカナに変換してマッチさせる
		if (!m->katakana_text.empty()) {
			std::vector<Part> parts;
			to_kana(text, &parts);
			for (size_t i = 0; i < parts.size(); i++) {
				bool match = parts[i].kana.text.find(m->katakana_text) != std::string::npos;
				if (!match) continue;
				// マッチした部分を追加
				size_t pos = parts[i].source.pos;
				size_t end = parts[i].source.end;
				Part part;
				part.match = true;
				part.source.pos = pos;
				part.source.end = end;
				part.source.text = text.substr(pos, end - pos);
				matched_list.push_back(part);
			}
		}
		// マッチした部分を位置順にソート
		std::sort(matched_list.begin(), matched_list.end(), [](ResultPart const &a, ResultPart const &b) {
			return a.pos < b.pos;
		});
		// マッチしていない部分を追加する関数
		auto AddNoMatch = [&ret, &text](size_t pos, size_t end){
			ResultPart part;
			part.match = false;
			part.pos = pos;
			part.end = end;
			part.text = text.substr(part.pos, part.end - part.pos);
			ret.parts.push_back(part);
		};
		// マッチした部分を順番に処理
		size_t pos = 0;
		for (const ResultPart &part : matched_list) {
			if (pos < part.pos) { // マッチした部分の前の部分
				AddNoMatch(pos, part.pos);
			}
			auto *back = ret.parts.empty() ? nullptr : &ret.parts.back();
			if (back && back->match && back->end >= part.pos) { // 前の部分と連続してマッチしているときは結合する
				size_t pos = back->pos; // 前の部分の開始位置
				size_t end = std::max(back->end, part.end); // 前の部分と今回の部分の終了位置のうち大きい方
				back->end = end; // 前の部分の終了位置を更新
				back->text = text.substr(pos, end - pos); // 前の部分のテキストを更新
			} else { // マッチした部分を追加
				ret.parts.push_back(part);
			}
			pos = part.end;
		}
		// 最後のフィルタ以降の部分
		if (pos < text.size()) {
			AddNoMatch(pos, text.size());
		}
		// マッチした部分が1つでもあれば、全体がマッチとみなす
		ret.match = !matched_list.empty();
	}
	return ret;
}

