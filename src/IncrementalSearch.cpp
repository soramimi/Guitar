#include "IncrementalSearch.h"
#include "ApplicationGlobal.h"
#include "common/joinpath.h"
#include "zip/zip.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <QDirIterator>
#include <QPainter>
#include <QStyleOptionViewItem>

#include "migemo.h"

#define MIGEMO_ABOUT "cmigemo - C/Migemo Library " MIGEMO_VERSION " Driver"
#define MIGEMODICT_NAME "migemo-dict"
#define MIGEMO_SUBDICT_MAX 8

struct IncrementalSearch::M {
	migemo *pmigemo = nullptr;
	std::string dict_path;
};

IncrementalSearch::IncrementalSearch()
	: m(new M)
{
}

IncrementalSearch::~IncrementalSearch()
{
	close();
	delete m;
}

void IncrementalSearch::init()
{
	m->dict_path = migemoDictPath();
}

bool IncrementalSearch::open()
{
	close();

	m->pmigemo = migemo_open(m->dict_path.c_str());
	if (!m->pmigemo) return false;

	char *subdict[MIGEMO_SUBDICT_MAX];
	int subdict_count = 0;

	memset(subdict, 0, sizeof(subdict));

	/* サブ辞書を読み込む */
	if (subdict_count > 0) {
		for (int i = 0; i < subdict_count; ++i) {
			if (subdict[i] == NULL || subdict[i][0] == '\0') continue;
			migemo_load(m->pmigemo, MIGEMO_DICTID_MIGEMO, subdict[i]);
		}
	}

	return true;
}

void IncrementalSearch::close()
{
	if (m->pmigemo) {
		migemo_close(m->pmigemo);
		m->pmigemo = nullptr;
	}
}

std::optional<std::string> IncrementalSearch::queryMigemo(char const *word)
{
	std::optional<std::string> ret;
	if (m->pmigemo) {
		unsigned char *ans = (unsigned char *)migemo_query(m->pmigemo, (unsigned char const *)word);
		if (ans) {
			ret = (char const *)ans;
		}
		migemo_release(m->pmigemo, ans);
	}
	return ret;
}

IncrementalSearch *IncrementalSearch::instance()
{
	return global->incremental_search();
}

bool IncrementalSearch::migemoEnabled()
{
	return global->appsettings.incremental_search_with_miegemo;
}

std::string IncrementalSearch::migemoDictDir()
{
	QString path = global->app_config_dir / "migemo";
	return path.toStdString();
}

std::string IncrementalSearch::migemoDictPath()
{
	return migemoDictDir() / MIGEMODICT_NAME;
}

bool IncrementalSearch::setupMigemoDict()
{
	QFile file(":/misc/migemo.zip"); // load from resource
	if (!file.open(QFile::ReadOnly)) return false;

	QByteArray data = file.readAll();
	if (data.size() == 0) {
		qDebug() << "Failed to load the zip file.";
		return false;
	}

	// extract dict files
	if (!zip::Zip::extract_from_data(data.data(), data.size(), global->app_config_dir.toStdString())) return false;

	// remove CR from migemo dict files
	QDirIterator it(global->app_config_dir / "migemo");
	while (it.hasNext()) {
		it.next();
		QFileInfo info = it.fileInfo();
		if (info.isFile()) {
			QFile file(info.absoluteFilePath());
			if (file.open(QFile::ReadWrite)) {
				file.seek(0);
				QByteArray data = file.readAll();
				if (data.isEmpty()) continue;
				char *src = data.data();
				char *end = src + data.size();
				char *dst = src;
				while (src < end) {
					if (*src == '\r') {
						src++;
						continue;
					}
					*dst++ = *src++;
				}
				file.seek(0);
				file.resize(0);
				file.write(data.data(), dst - data.data());
				file.close();
			}
		}
	}

	return true;
}

static void deleteTree(QString const &dir)
{
	QDir d(dir);
	if (!d.exists()) return;
	for (QFileInfo const &fi : d.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
		if (fi.isDir()) {
			deleteTree(fi.filePath());
		} else {
			d.remove(fi.fileName());
		}
	}
	d.rmdir(".");
}

void IncrementalSearch::deleteMigemoDict()
{
	QString dir = global->app_config_dir / "migemo";
	deleteTree(dir);
}


//


MigemoFilter::MigemoFilter(const QString &filtertext)
{
	makeFilter(filtertext);
}

bool MigemoFilter::isEmpty() const
{
	return text_.isEmpty();
}

void MigemoFilter::makeFilter(const QString &filtertext)
{
	text_ = filtertext;
	if (IncrementalSearch::instance()->migemoEnabled()) {
		if (filtertext.size() > 0) {
			auto s = IncrementalSearch::instance()->queryMigemo(filtertext.toStdString().c_str());
			if (s) {
				re_ = std::make_shared<QRegularExpression>(QString::fromStdString(*s), QRegularExpression::CaseInsensitiveOption);
			}
		}
	}
}

AbstractIncrementalSearchFilter::Result MigemoFilter::match(QString const &text) const
{
	if (isEmpty()) {
		Result ret;
		ret.match = true; // フィルターが空の場合は常にtrue
		return ret;
	}
	QString text2 = text;
	if (re_.get()) { // 正規表現が有効な場合
		text2 = normalizeText(text2);
		QRegularExpressionMatch m;
		if (text2.contains(*re_, &m)) {
			Result ret;
			ret.match = true;
			ret.pos = m.capturedStart();
			ret.end = m.capturedEnd();
			// ret.part2;
			{
				Part2 part;
				part.source.pos = 0;
				part.source.end = ret.pos;
				part.source.text = text.mid(0, ret.pos).toStdString();
				part.match = false;
				ret.part2.push_back(part);
			}
			{
				Part2 part;
				part.source.pos = ret.pos;
				part.source.end = ret.end;
				part.source.text = text.mid(ret.pos, ret.end - ret.pos).toStdString();
				part.match = true;
				ret.part2.push_back(part);
			}
			{
				Part2 part;
				part.source.pos = ret.end;
				part.source.end = text.size();
				part.source.text = text.mid(ret.end).toStdString();
				part.match = false;
				ret.part2.push_back(part);
			}
			return ret;
		}
		return {};
	}
	// 正規表現が無効な場合
	Result ret;
	ret.match = text2.indexOf(text2, 0, Qt::CaseInsensitive) >= 0;
	return ret;
}

int AbstractIncrementalSearchFilter::u16ncmp(ushort const *s1, ushort const *s2, int n)
{
	for (int i = 0; i < n; i++) {
		ushort c1 = s1[i];
		ushort c2 = s2[i];
		if (c1 < 128) c1 = toupper(c1);
		if (c2 < 128) c2 = toupper(c2);
		if (c1 != c2) {
			return c1 - c2;
		}
	}
	return 0;
}

void IncrementalSearch::fillFilteredBG(QPainter *painter, const QRect &rect)
{
	painter->fillRect(rect, QColor(128, 128, 128, 64));
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

QString AbstractIncrementalSearchFilter::normalizeText(QString s)
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

void IncrementalSearch::drawText_filted(QPainter *painter, const QStyleOptionViewItem &opt, const QRect &rect, IncrementalSearchFilter const &filter)
{
	QString text = opt.text;

	std::vector<std::tuple<QString, bool>> list;

	AbstractIncrementalSearchFilter::Result r = filter.match(text);
	for (AbstractIncrementalSearchFilter::Part2 const &part2 : r.part2) {
		list.push_back(std::make_tuple(QString::fromStdString(part2.source.text), part2.match));
	}

	int x = rect.x();
	for (auto [s, f] : list) {
		int w = painter->fontMetrics().size(Qt::TextSingleLine, s).width();
		QRect r = rect;
		r.setLeft(x);
		r.setWidth(w);
		if (f) { // フィルターの部分をハイライト
			QColor color = opt.palette.color(QPalette::Highlight);
			color.setAlpha(128);
			painter->fillRect(r, color);
		}
		drawText(painter, opt, r, s);
		x += w;
	}
}


std::string MecabFilter::to_kana(const std::string &text, std::vector<Part2> *out)
{
	std::string kana;
	std::vector<MeCaSearch::Part> parts = global->meca.parse(text);
	size_t pos = 0;
	for (MeCaSearch::Part const &part : parts) {
		Part2 part2;
		part2.source.text = text.substr(part.offset, part.length);
		part2.source.pos = kana.size();
		part2.source.end = part.offset + part.length;
		auto end = pos + part.text.size();
		part2.kana.text = part.text;
		part2.kana.pos = pos;
		part2.kana.end = end;
		pos = end;
		out->push_back(part2);
		kana.append(part.text);
	}
	return kana;
}

void MecabFilter::makeFilter(const QString &filtertext)
{
	original_text_ = filtertext.toStdString();
	katakana_text_ = global->meca.convert_roman_to_katakana(original_text_);
}

MecabFilter::MecabFilter(const QString &filtertext)
{
	makeFilter(filtertext);
}

bool MecabFilter::isEmpty() const
{
	// return katakana_text_.empty();
	return original_text_.empty();
}

AbstractIncrementalSearchFilter::Result MecabFilter::match(QString const &text) const
{
	Result ret;
	{
		std::string text2 = text.toStdString();
		char const *p = misc::stristr(text2.c_str(), original_text_.c_str());
		if (p) {
			size_t pos = p - text2.c_str();
			ret.match = true;
			ret.pos = pos;
			ret.end = pos + original_text_.size();
			if (ret.pos < ret.end) {
				{
					Part2 part;
					part.source.pos = 0;
					part.source.end = ret.pos;
					part.source.text = text2.substr(0, ret.pos);
					part.match = false;
					ret.part2.push_back(part);
				}
				{
					Part2 part;
					part.source.pos = ret.pos;
					part.source.end = ret.end;
					part.source.text = text2.substr(ret.pos, ret.end - ret.pos);
					part.match = true;
					ret.part2.push_back(part);
				}
				{
					Part2 part;
					part.source.pos = ret.end;
					part.source.end = text2.size();
					part.source.text = text2.substr(ret.end);
					part.match = false;
					ret.part2.push_back(part);
				}
				return ret;
			}
		}
	}
	if (!katakana_text_.empty()) {
		std::vector<Part2> part2;
		std::string kana = to_kana(text.toStdString(), &part2);
		char const *p = strstr(kana.c_str(), katakana_text_.c_str());
		if (p) {
			size_t pos = p - kana.c_str();
			size_t end = pos + katakana_text_.size();
			ret.match = true;
			ret.pos = pos;
			for (size_t i = 0; i < part2.size(); i++) {
				if (pos <= part2[i].kana.pos && part2[i].kana.pos < end) {
					ret.end = part2[i].kana.end;
					part2[i].match = true;
				}
			}
		}
		ret.part2 = part2;
	}
	return ret;
}
