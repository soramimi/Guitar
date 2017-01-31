#ifndef GITOBJECTMANAGER_H
#define GITOBJECTMANAGER_H

#include <QString>
#include "GitPack.h"
#include "GitPackIdxV2.h"

class GitPackIdxV2;

class Git;
typedef std::shared_ptr<Git> GitPtr;

class GitObjectManager {
	friend class GitObjectCache;
private:
	GitPtr g;
	QMutex mutex;
	QString subdir_git_objects;
	QString subdir_git_objects_pack;
	std::vector<GitPackIdxPtr> git_idx_list;

	QString workingDir();

	static void applyDelta(QByteArray *base, QByteArray *delta, QByteArray *out);
	static bool loadPackedObject(GitPackIdxPtr idx, QIODevice *packfile, const GitPackIdxItem *item, GitPack::Object *out);
	bool extractObjectFromPackFile(GitPackIdxPtr idx, const GitPackIdxItem *item, GitPack::Object *out);
	bool extractObjectFromPackFile(const QString &id, QByteArray *out);
	void loadIndexes();
	QString findObjectPath(const QString &id);
	bool loadObject(const QString &id, QByteArray *out);
	GitPtr git()
	{
		return g;
	}
public:
	GitObjectManager();
	void setup(GitPtr g);
	bool catFile(const QString &id, QByteArray *out);
	void clearIndexes();
	bool extractDebug(GitPackIdxPtr idx, const GitPackIdxItem *item, GitPack::Object *out);
};

class GitObjectCache {
public:
	struct Item {
		QString id;
		QByteArray ba;
	};
private:
	GitObjectManager object_manager;
	typedef std::shared_ptr<Item> ItemPtr;
	std::vector<ItemPtr> items;
	size_t size() const;
public:
	GitPtr git()
	{
		return object_manager.git();
	}

	void setup(GitPtr g);
	QByteArray catFile(QString const &id);
};

#endif // GITOBJECTMANAGER_H
