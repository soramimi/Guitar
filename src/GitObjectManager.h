#ifndef GITOBJECTMANAGER_H
#define GITOBJECTMANAGER_H

#include <QMutex>
#include <QString>
#include "GitPack.h"
#include "GitPackIdxV2.h"
#include <map>
#include "common/joinpath.h"

class GitPackIdxV2;

class Git;
using GitPtr = std::shared_ptr<Git>;

class GitObjectManager {
	friend class GitObjectCache;
private:
	// GitPtr g;
	QMutex mutex;
	QString subdir_git_objects;
	QString subdir_git_objects_pack;
	std::vector<GitPackIdxPtr> git_idx_list;

	static QString workingDir(GitPtr g);

	static void applyDelta(QByteArray const *base, QByteArray const *delta, QByteArray *out);
	static bool loadPackedObject(GitPackIdxPtr const &idx, QIODevice *packfile, GitPackIdxItem const *item, GitPack::Object *out);
	static bool extractObjectFromPackFile(GitPackIdxPtr const &idx, GitPackIdxItem const *item, GitPack::Object *out);
	bool extractObjectFromPackFile(GitPtr g, const Git::CommitID &id, QByteArray *out, Git::Object::Type *type);
	void loadIndexes(GitPtr g);
	QString findObjectPath(GitPtr g, const Git::CommitID &id);
	bool loadObject(GitPtr g, const Git::CommitID &id, QByteArray *out, Git::Object::Type *type);
	void init();
public:
	GitObjectManager();
	GitObjectManager(GitPtr g);
	void setup(GitPtr g);
	bool catFile(GitPtr g, const Git::CommitID &id, QByteArray *out, Git::Object::Type *type);
	void clearIndexes();

	static QStringList findObject(const QString &id, const QString &repo_local_dir);
};

class GitObjectCache {
public:
	struct Item {
		Git::CommitID id;
		QByteArray ba;
		Git::Object::Type type;
	};
private:
	GitObjectManager object_manager;
	using ItemPtr = std::shared_ptr<Item>;
	std::vector<ItemPtr> items;
	std::map<QString, Git::CommitID> revparsemap;
	size_t size() const;
public:
	// GitPtr git()
	// {
	// 	return object_manager.git();
	// }
	// GitPtr git(Git::SubmoduleItem const &submod)
	// {
	// 	return object_manager.git(submod);
	// }

	void setup(GitPtr g);
	Git::CommitID revParse(GitPtr g, QString const &name);
	Git::Object catFile(GitPtr g, const Git::CommitID &id);
	Git::CommitID getCommitIdFromTag(GitPtr g, const QString &tag);

	Git::CommitID const &item_id(int i) const
	{
		return items[i]->id;
	}
};

class GitCommit {
public:
	QString tree_id;
	QStringList parents;

	static bool parseCommit(GitPtr g, GitObjectCache *objcache, Git::CommitID const &id, GitCommit *out);
};

struct GitTreeItem {
	enum Type {
		UNKNOWN,
		TREE,
		BLOB,
		COMMIT,
	};
	Type type = UNKNOWN;
	QString name;
	QString id;
	QString mode;

	QString to_string_() const
	{
		QString t;
		switch (type) {
		case TREE: t = "TREE"; break;
		case BLOB: t = "BLOB"; break;
		}
		return QString("GitTreeItem:{ %1 %2 %3 %4 }").arg(t).arg(id).arg(mode).arg(name);
	}
};

using GitTreeItemList = QList<GitTreeItem>;

class GitCommitTree {
private:
	GitObjectCache *objcache;
	GitTreeItemList root_item_list;

	std::map<QString, GitTreeItem> blob_map;
	std::map<QString, QString> tree_id_map;

	GitPtr git();

	QString lookup_(GitPtr g, QString const &file, GitTreeItem *out);
public:
	GitCommitTree(GitObjectCache *objcache);

	QString lookup(GitPtr g, QString const &file);
	bool lookup(GitPtr g, QString const &file, GitTreeItem *out);

	void parseTree(GitPtr g, QString const &tree_id);
	QString parseCommit(GitPtr g, QString const &commit_id);

	GitTreeItemList const *treelist() const
	{
		return &root_item_list;
	}
};

void parseGitTreeObject(QByteArray const &ba, const QString &path_prefix, GitTreeItemList *out);
bool parseGitTreeObject(GitPtr g, GitObjectCache *objcache, QString const &commit_id, QString const &path_prefix, GitTreeItemList *out);

#endif // GITOBJECTMANAGER_H
