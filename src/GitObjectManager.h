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
	GitPtr g;
	QMutex mutex;
	QString subdir_git_objects;
	QString subdir_git_objects_pack;
	std::vector<GitPackIdxPtr> git_idx_list;

	QString workingDir();

	static void applyDelta(QByteArray const *base, QByteArray const *delta, QByteArray *out);
	static bool loadPackedObject(GitPackIdxPtr const &idx, QIODevice *packfile, GitPackIdxItem const *item, GitPack::Object *out);
	bool extractObjectFromPackFile(GitPackIdxPtr const &idx, GitPackIdxItem const *item, GitPack::Object *out);
	bool extractObjectFromPackFile(const Git::CommitID &id, QByteArray *out, Git::Object::Type *type);
	void loadIndexes();
	QString findObjectPath(const Git::CommitID &id);
	bool loadObject(const Git::CommitID &id, QByteArray *out, Git::Object::Type *type);
	GitPtr git()
	{
		return g->dup();
	}
	GitPtr git(Git::SubmoduleItem const &submod)
	{
		GitPtr g2 = g->dup();
		g2->setWorkingRepositoryDir(g->workingDir(), submod.path, g->sshKey());
		return g2;
	}
	void init();
public:
	GitObjectManager();
	GitObjectManager(GitPtr g);
	void setup(GitPtr g);
	bool catFile(const Git::CommitID &id, QByteArray *out, Git::Object::Type *type);
	void clearIndexes();
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
	GitPtr git()
	{
		return object_manager.git();
	}
	GitPtr git(Git::SubmoduleItem const &submod)
	{
		return object_manager.git(submod);
	}

	void setup(GitPtr g);
	Git::CommitID revParse(QString const &name);
	Git::Object catFile(const Git::CommitID &id);
	Git::CommitID getCommitIdFromTag(const QString &tag);

	Git::CommitID const &item_id(int i) const
	{
		return items[i]->id;
	}
};

class GitCommit {
public:
	QString tree_id;
	QStringList parents;

	static bool parseCommit(GitObjectCache *objcache, Git::CommitID const &id, GitCommit *out);
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

	QString lookup_(QString const &file, GitTreeItem *out);
public:
	GitCommitTree(GitObjectCache *objcache);

	QString lookup(QString const &file);
	bool lookup(QString const &file, GitTreeItem *out);

	void parseTree(QString const &tree_id);
	QString parseCommit(QString const &commit_id);

	GitTreeItemList const *treelist() const
	{
		return &root_item_list;
	}
};

QString lookupFileID(GitObjectCache *objcache, QString const &commit_id, QString const &file);

void parseGitTreeObject(QByteArray const &ba, const QString &path_prefix, GitTreeItemList *out);
bool parseGitTreeObject(GitObjectCache *objcache, QString const &commit_id, QString const &path_prefix, GitTreeItemList *out);


#endif // GITOBJECTMANAGER_H
