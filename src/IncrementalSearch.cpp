#include "IncrementalSearch.h"
#include "ApplicationGlobal.h"
#include "common/joinpath.h"
#include <QDebug>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <QFile>
#include <QDir>
#include "zip/zip.h"

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
	return zip::Zip::extract_from_data(data.data(), data.size(), global->app_config_dir.toStdString());
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

