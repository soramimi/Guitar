#include "GitObjectManager.h"
#include "Git.h"
#include "GitPack.h"

#include <QBuffer>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include "joinpath.h"
#include "misc.h"

GitObjectManager::GitObjectManager(const QString &workingdir)
	: working_dir(workingdir)
{
}

bool GitObjectManager::loadObject_(QString const &id, QByteArray *out)
{
	QString path;
	Git::findObjectID(working_dir, id, &path);
	if (!path.isEmpty()) {
		QFile file(path);
		if (file.open(QFile::ReadOnly)) {
			if (GitPack::decompress(&file, true, GitPack::Type::UNKNOWN, 1000000000, out)) {
				return true;
			}
		}
	}
	return false;
}

static bool load(QString const &path, GitPackIdxV2 const *idx, size_t i, QByteArray *out)
{
	GitPackIdxV2::Item const *item = idx->item(i);
	if (item) {
		GitPack::Object obj;
		if (GitPack::load(path, item, &obj)) {
			if (obj.type == GitPack::Type::OFS_DELTA) {
				if (i > 0) {
					QByteArray ba;
					if (load(path, idx, i - 1, &ba)) {
						qDebug() << ba.size();
						return true;
					}
				}
			}
			*out = std::move(obj.content);
			return true;
		}
	}
	return false;
}

bool GitObjectManager::extractObjectFromPackFile_(QString const &id, QByteArray *out)
{
	struct Entry {
		QString path;
		size_t index;
	};
	GitPackIdxV2 idx;
	std::vector<Entry> entries;
	{
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
						Entry e;
						QString packfilename = it.fileInfo().baseName() + ".pack";
						e.path = working_dir / subdir / packfilename;
						e.index = i;
						entries.push_back(e);
					}
				}
			}
		}
	}
	if (entries.size() == 1) {
		Entry const &e = entries.front();
		GitPackIdxV2::Item const *item = idx.item(e.index);
//		GitPack::Object obj;
		if (id.startsWith("f98089")) {
			qDebug() << "---";
		}
		if (load(e.path, &idx, e.index, out)) {
//			*out = std::move(obj.content);
			return true;
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

