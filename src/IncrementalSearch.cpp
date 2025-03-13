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


MigemoFilter::MigemoFilter(const QString &text)
	: text(text)
	, re_(std::make_shared<QRegularExpression>(text, QRegularExpression::CaseInsensitiveOption))
{
}

bool MigemoFilter::isEmpty() const
{
	return text.isEmpty();
}

void MigemoFilter::makeFilter(const QString &filtertext)
{
	text = filtertext;
	if (IncrementalSearch::instance()->migemoEnabled()) {
		if (filtertext.size() >= 2) {
			bool re = true;
			int vowel = 0; // 母音の数
			for (QChar c : filtertext) {
				if (c.isLetter()) {
					if (c == 'a' || c == 'i' || c == 'u' || c == 'e' || c == 'o') {
						vowel++;
					}
				} else if (c.isDigit()) {
					// thru
				} else {
					re = false;
					break;
				}
			};
			if (re && vowel >= 1) { // 全体が2文字以上の英数字で、母音が1文字以上含まれる場合
				auto s = IncrementalSearch::instance()->queryMigemo(filtertext.toStdString().c_str());
				if (s) {
					re_ = std::make_shared<QRegularExpression>(QString::fromStdString(*s), QRegularExpression::CaseInsensitiveOption);
					qDebug() << filtertext << *re_;
				}
			}
		}
	}
}

bool MigemoFilter::match(QString text)
{
	if (isEmpty()) return true; // フィルターが空の場合は常にtrue
	if (re_.get()) { // 正規表現が有効な場合
		text = normalizeText(text);
		if (text.contains(*re_)) return true;
		return false;
	}
	// 正規表現が無効な場合
	return text.indexOf(text, 0, Qt::CaseInsensitive) >= 0;
}

int MigemoFilter::u16ncmp(ushort const *s1, ushort const *s2, int n)
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

QString MigemoFilter::normalizeText(QString s)
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
