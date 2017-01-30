#ifndef GITOBJECTMANAGER_H
#define GITOBJECTMANAGER_H

#include <QString>
#include "GitPack.h"
#include "GitPackIdxV2.h"

class GitPackIdxV2;

class GitObjectManager {
private:
	QString subdir_git_objects;
	QString subdir_git_objects_pack;
	QString working_dir;
	std::vector<GitPackIdxPtr> git_idx_list;

	static void applyDelta(QByteArray *base, QByteArray *delta, QByteArray *out);
	static bool loadPackedObject(GitPackIdxPtr idx, QIODevice *packfile, const GitPackIdxItem *item, GitPack::Object *out);
	bool extractObjectFromPackFile(GitPackIdxPtr idx, const GitPackIdxItem *item, GitPack::Object *out);
	bool extractObjectFromPackFile(const QString &id, QByteArray *out);
	void loadIndexes();
	QString findObjectPath(const QString &id);
	bool loadObject(const QString &id, QByteArray *out);
public:
	GitObjectManager(QString const &workingdir);
	bool catFile(const QString &id, QByteArray *out);
	void clearIndexes();
};

#endif // GITOBJECTMANAGER_H
