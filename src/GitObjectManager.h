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

class GitObjectManager {
	friend class GitObjectCache;
private:
	// QMutex mutex;
	QString subdir_git_objects;
	QString subdir_git_objects_pack;
	std::vector<GitPackIdxPtr> git_idx_list;

	static void applyDelta(QByteArray const *base, QByteArray const *delta, QByteArray *out);
	static bool loadPackedObject(GitPackIdxPtr const &idx, QIODevice *packfile, GitPackIdxItem const *item, GitPack::Object *out);
	bool extractObjectFromPackFile(GitPackIdxPtr const &idx, GitPackIdxItem const *item, GitPack::Object *out);
	bool extractObjectFromPackFile(GitRunner g, const Git::Hash &id, QByteArray *out, Git::Object::Type *type);
	void loadIndexes(GitRunner g);
	QString findObjectPath(GitRunner g, const Git::Hash &id);
	bool loadObject(GitRunner g, const Git::Hash &id, QByteArray *out, Git::Object::Type *type);
	void init();
public:
	GitObjectManager();
	void setup();
	bool catFile(GitRunner g, const Git::Hash &id, QByteArray *out, Git::Object::Type *type);
	void clearIndexes();

	static QStringList findObject(const QString &id, const QString &repo_local_dir);
};

class GitObjectCache {
public:
	struct Item {
		Git::Hash id;
		QByteArray ba;
		Git::Object::Type type;
	};
private:
	GitObjectManager object_manager;
	using ItemPtr = std::shared_ptr<Item>;
	std::vector<ItemPtr> items;
	std::map<QString, Git::Hash> revparsemap;
	size_t size() const;
public:
	void clear();
	Git::Hash revParse(GitRunner g, QString const &name);
	Git::Object catFile(GitRunner g, const Git::Hash &id);

	Git::Hash const &item_id(int i) const
	{
		return items[i]->id;
	}
};

class GitCommit {
public:
	QString tree_id;
	QStringList parents;

	static bool parseCommit(GitRunner g, GitObjectCache *objcache, Git::Hash const &id, GitCommit *out);
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

	QString lookup_(GitRunner g, QString const &file, GitTreeItem *out);
public:
	GitCommitTree(GitObjectCache *objcache);

	QString lookup(GitRunner g, QString const &file);
	bool lookup(GitRunner g, QString const &file, GitTreeItem *out);

	void parseTree(GitRunner g, QString const &tree_id);
	QString parseCommit(GitRunner g, QString const &commit_id);

	GitTreeItemList const *treelist() const
	{
		return &root_item_list;
	}
};

void parseGitTreeObject(QByteArray const &ba, const QString &path_prefix, GitTreeItemList *out);
bool parseGitTreeObject(GitRunner g, GitObjectCache *objcache, QString const &commit_id, QString const &path_prefix, GitTreeItemList *out);

#endif // GITOBJECTMANAGER_H
