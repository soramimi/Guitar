#include "LibMigemo.h"
#include "migemo.h"
#include <cstring>
#include "ApplicationGlobal.h"
#include "common/joinpath.h"
#include "zip/zip.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QStandardPaths>

struct LibMigemo::Private {
	migemo *pmigemo = nullptr;
	std::string dict_path;
};

#define MIGEMO_ABOUT "cmigemo - C/Migemo Library " MIGEMO_VERSION " Driver"
#define MIGEMODICT_NAME "migemo-dict"
#define MIGEMO_SUBDICT_MAX 8

LibMigemo::LibMigemo()
	: m(new Private)
{
}

LibMigemo::~LibMigemo()
{
	close();
	delete m;
}

void LibMigemo::init()
{
	m->dict_path = migemoDictPath();
}

bool LibMigemo::open()
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

void LibMigemo::close()
{
	if (m->pmigemo) {
		migemo_close(m->pmigemo);
		m->pmigemo = nullptr;
	}
}

std::optional<std::string> LibMigemo::queryMigemo(char const *word)
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

LibMigemo *LibMigemo::instance()
{
	return global->incremental_search();
}

bool LibMigemo::migemoEnabled()
{
	return global->appsettings.incremental_search_with_miegemo;
}

std::string LibMigemo::migemoDictDir()
{
	QString path = global->app_config_dir / "migemo";
	return path.toStdString();
}

std::string LibMigemo::migemoDictPath()
{
	return migemoDictDir() / MIGEMODICT_NAME;
}

bool LibMigemo::setupMigemoDict()
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

void LibMigemo::deleteMigemoDict()
{
	QString dir = global->app_config_dir / "migemo";
	deleteTree(dir);
}

