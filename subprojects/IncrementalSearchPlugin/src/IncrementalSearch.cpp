
#include "MySudachi.h"
#include "IncrementalSearch.h"
#include <MyMecab.h>
#include <QDebug>
#include "romaji.h"

namespace misc {

static inline int stricmp(char const *s1, char const *s2)
{
#ifdef _WIN32
	return ::stricmp(s1, s2);
#else
	return ::strcasecmp(s1, s2);
#endif
}

static inline int strnicmp(char const *s1, char const *s2, size_t n)
{
#ifdef _WIN32
	return ::strnicmp(s1, s2, n);
#else
	return ::strncasecmp(s1, s2, n);
#endif
}

static inline int stricmp(std::string_view const &s1, std::string_view const &s2)
{
	size_t n1 = s1.size();
	size_t n2 = s2.size();
	size_t n = std::min(n1, n2);
	if (n > 0) {
		int i = strnicmp(s1.data(), s2.data(), n);
		if (i != 0) return i;
	}
	if (n1 < n2) return -1;
	if (n1 > n2) return 1;
	return 0;
}

static inline char const *stristr(const char *haystack, const char *needle)
{
	size_t needle_len = strlen(needle);
	for (const char *p = haystack; *p; p++) {
		if (strnicmp(p, needle, needle_len) == 0) {
			return p;
		}
	}
	return nullptr;
}

}

struct IncrementalSearch::Private {
	MyMecab mecab;
#ifdef USE_SUDACHI
	MySudachi sudachi;
#endif
};


IncrementalSearch::IncrementalSearch()
	: m(new Private())
{
}

incrementalsearch::Engine *IncrementalSearch::mecab()
{
	return &m->mecab;
}

const incrementalsearch::Engine *IncrementalSearch::mecab() const
{
	return &m->mecab;
}

bool IncrementalSearch::open()
{
	try {
		if (!mecab()->open("/dummy/")) {
			throw QString("Failed to open IncrementalSearchPlugin. This may cause some features to not work properly.");
		}
		// test
		{
			std::string s = convertRomajiToKatakana("wagahaihanekodearu");
			if (s != "ワガハイハネコデアル") {
				throw QString("Failed to convert romaji to katakana: " + QString::fromStdString(s));
			}
			auto parts = mecab()->parse("吾輩は猫である");
			if (parts.size() != 5) {
				throw QString("Failed to parse sentence with IncrementalSearchPlugin. Expected 5 parts, got " + parts.size());
			} else {
				std::vector<std::string> expected = {"ワガハイ", "ハ", "ネコ", "デ", "アル"};
				for (size_t i = 0; i < parts.size(); i++) {
					if (parts[i].text != expected[i]) {
						throw QString("Failed to parse sentence with IncrementalSearchPlugin. Part " + QString::number(i) + ": expected \"" + QString::fromStdString(expected[i]) + "\", got \"" + QString::fromStdString(parts[i].text) + "\"");
					}
				}
			}
		}
		{
			// カスタム辞書のテスト
			// 「みみの」はオリジナル辞書には存在しないので、
			// Custom.csv に「みみの」を定義している。
			auto parts = mecab()->parse("かわいいみみのちゃん");
			if (parts.size() != 3) {
				// throw QString("Failed to parse sentence with IncrementalSearchPlugin. Expected 3 parts, got " + QString::number(parts.size()));
				qDebug() << "Failed to parse sentence with IncrementalSearchPlugin. Expected 3 parts, got " << parts.size() << ". This may be caused by MeCab not being properly initialized. Please check if the dictionary files are correctly set up.";
			} else {
				std::vector<std::string> expected = {"カワイイ", "ミミノ", "チャン"};
				for (size_t i = 0; i < parts.size(); i++) {
					if (parts[i].text != expected[i]) {
						throw QString("Failed to parse sentence with IncrementalSearchPlugin. Part " + QString::number(i) + ": expected \"" + QString::fromStdString(expected[i]) + "\", got \"" + QString::fromStdString(parts[i].text) + "\"");
					}
				}
			}
		}
		{
			IncrementalSearchFilter filter = makeFilter("neko");
			incrementalsearch::Result r = match("吾輩は猫である", filter);
			if (!r) {
				throw QString("Failed to match romaji filter with IncrementalSearchPlugin. Expected match, got no match.");
			}
			if (!(r.parts.size() == 3)) {
				throw QString("Failed to match romaji filter with IncrementalSearchPlugin. Expected 3 parts, got " + QString::number(r.parts.size()));
			}
			if (!(r.parts[0].text == "吾輩は" && !r.parts[0].match)) {
				throw QString("Failed to match romaji filter with IncrementalSearchPlugin. Expected part 0 to be \"吾輩は\", got \"" + QString::fromStdString(r.parts[0].text) + "\"");
			}
			if (!(r.parts[1].text == "猫" && r.parts[1].match)) {
				throw QString("Failed to match romaji filter with IncrementalSearchPlugin. Expected part 1 to be \"猫\", got \"" + QString::fromStdString(r.parts[1].text) + "\"");
			}
			if (!(r.parts[2].text == "である" && !r.parts[2].match)) {
				throw QString("Failed to match romaji filter with IncrementalSearchPlugin. Expected part 2 to be \"である\", got \"" + QString::fromStdString(r.parts[2].text) + "\"");
			}
		}
	} catch (QString const &err) {
		qDebug() << "Failed to initialize IncrementalSearchPlugin: " << err;
		mecab()->close();
		return false;
	}
	return true;
}

IncrementalSearch::~IncrementalSearch()
{
	mecab()->close();
	delete m;
}

std::string IncrementalSearch::to_kana(const std::string &text, std::vector<incrementalsearch::Element> *out) const
{
	std::string kana;
	std::vector<incrementalsearch::Part> parts = mecab()->parse(text);
	size_t pos = 0;
	for (incrementalsearch::Part const &part : parts) {
		incrementalsearch::Element item;
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

std::string IncrementalSearch::convertRomajiToKatakana(std::string const &in)
{
	return convert_romaji_to_katakana(in);
}

IncrementalSearchFilter IncrementalSearch::makeFilter(std::string const &filtertext)
{
	IncrementalSearchFilter filter;
	filter.original_text = filtertext;
	filter.katakana_text = convert_romaji_to_katakana(filtertext);
	return filter;
}

incrementalsearch::Result IncrementalSearch::match(std::string const &text, const IncrementalSearchFilter &filter) const
{
	using namespace incrementalsearch;
	Result ret;
	if (filter.original_text.empty()) {
		// フィルタ文字列なしは全体一致とみなす
		Result::Part part;
		part.match = false;
		part.pos = 0;
		part.end = text.size();
		part.text = text;
		ret.parts.push_back(part);
		ret.match = true;
	} else {
		std::vector<Result::Part> matched_list;
		// フィルタ文字列をそのままマッチさせる
		char const *ptr = text.c_str();
		while (ptr) {
			char const *match = misc::stristr(ptr, filter.original_text.c_str());
			if (!match) break;
			// マッチした部分を追加
			Element part;
			part.match = true;
			part.source.pos = match - text.c_str();
			part.source.end = part.source.pos + filter.original_text.size();
			part.source.text = text.substr(part.source.pos, filter.original_text.size());
			matched_list.push_back(part);
			ptr = match + filter.original_text.size();
		}
		// 文字列をカタカナに変換してマッチさせる
		if (!filter.katakana_text.empty()) {
			std::vector<Element> parts;
			std::string kana = to_kana(text, &parts);
			(void)kana;
			for (size_t i = 0; i < parts.size(); i++) {
				bool match = parts[i].kana.text.find(filter.katakana_text) != std::string::npos;
				if (!match) continue;
				// マッチした部分を追加
				size_t pos = parts[i].source.pos;
				size_t end = parts[i].source.end;
				Element part;
				part.match = true;
				part.source.pos = pos;
				part.source.end = end;
				part.source.text = text.substr(pos, end - pos);
				matched_list.push_back(part);
			}
		}
		// マッチした部分を位置順にソート
		std::sort(matched_list.begin(), matched_list.end(), [](Result::Part const &a, Result::Part const &b) {
			return a.pos < b.pos;
		});
		// マッチしていない部分を追加する関数
		auto AddNoMatch = [&ret, &text](size_t pos, size_t end){
			Result::Part part;
			part.match = false;
			part.pos = pos;
			part.end = end;
			part.text = text.substr(part.pos, part.end - part.pos);
			ret.parts.push_back(part);
		};
		// マッチした部分を順番に処理
		size_t pos = 0;
		for (const Result::Part &part : matched_list) {
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

