#include "GitObjectManager.h"
#include "Git.h"
#include "GitPack.h"

#include <QBuffer>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include "joinpath.h"

GitObjectManager::GitObjectManager(const QString &workingdir)
	: working_dir(workingdir)
{
}

bool GitObjectManager::loadObject_(QString const &id, QByteArray *out)
{
//	qDebug() << Q_FUNC_INFO;
	QString path;
	Git::findObjectID(working_dir, id, &path);
	if (!path.isEmpty()) {
//		QFileInfo info(path);
		QFile file(path);
		if (file.open(QFile::ReadOnly)) {
//			QByteArray in = file.readAll();
//			file.close();
//			QBuffer src(&in);
//			src.open(QBuffer::ReadOnly);
			if (GitPack::decompress(&file, true, GitPack::Type::UNKNOWN, 1000000000, out)) {
				return true;
			}
		}
	}
	return false;
}

bool GitObjectManager::extractObjectFromPackFile_(QString const &id, QByteArray *out)
{
//	qDebug() << Q_FUNC_INFO;
	GitPackIdxV2 idx;
	QString subdir = ".git/objects/pack";
	QString path = working_dir / subdir;
	QDirIterator it(path, { "pack-*.idx" }, QDir::Files | QDir::Readable);
	while (it.hasNext()) {
		it.next();
		if (idx.parse(it.filePath())) {
			size_t n = idx.count();
			for (size_t i = 0; i < n; i++) {
				GitPackIdxV2::Item const *item = idx.item(i);
				if (item && item->id.startsWith(id)) {
					QString packfile = it.fileInfo().baseName() + ".pack";
					QString path = working_dir / subdir / packfile;
					GitPack::Object obj;
					if (id.startsWith("f98089")) {
						qDebug();
					}
					if (GitPack::load(path, item, &obj)) {
						*out = obj.content;
						char const *p = out->data();
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool GitObjectManager::loadObjectFile(const QString &id, QByteArray *out)
{
	if (loadObject_(id, out)) return true;
	if (extractObjectFromPackFile_(id, out)) return true;
	return false;
}

