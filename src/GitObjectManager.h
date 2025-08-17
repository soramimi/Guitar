#ifndef GITOBJECTMANAGER_H
#define GITOBJECTMANAGER_H

#include "GitRunner.h"

class GitPackIdxV2;
struct GitPackIdxItem;
struct GitPackObject;

class GitObjectManager {
	friend class GitObjectCache;
private:
	struct Private;
	Private *m;

	static void applyDelta(QByteArray const *base, QByteArray const *delta, QByteArray *out);
	static bool loadPackedObject(std::shared_ptr<GitPackIdxV2> const &idx, QIODevice *packfile, GitPackIdxItem const *item, GitPackObject *out);
	bool extractObjectFromPackFile(std::shared_ptr<GitPackIdxV2> const &idx, GitPackIdxItem const *item, GitPackObject *out);
	bool extractObjectFromPackFile(GitRunner g, const GitHash &id, QByteArray *out, GitObject::Type *type, std::mutex *mutex);
	void loadIndexes(GitRunner g, std::mutex *mutex);
	QString findObjectPath(GitRunner g, const GitHash &id);
	bool loadObject(GitRunner g, const GitHash &id, QByteArray *out, GitObject::Type *type);
	void init();
public:
	GitObjectManager(std::mutex *mutex);
	GitObjectManager(GitObjectManager const &other);
	GitObjectManager(GitObjectManager &&other) = delete;
	~GitObjectManager();
	GitObjectManager &operator = (GitObjectManager const &other);
	void setup();
	bool catFile(GitRunner g, const GitHash &id, QByteArray *out, GitObject::Type *type);
	void clearIndexes();

	static QStringList findObject(const QString &id, const QString &repo_local_dir);
};

class GitObjectCache {
public:
	struct Item {
		GitHash id;
		QByteArray ba;
		GitObject::Type type;
	};
private:
	std::mutex *mutex_ = nullptr;
	GitObjectManager object_manager_;
	using ItemPtr = std::shared_ptr<Item>;
	std::vector<ItemPtr> items_;
	std::map<QString, GitHash> rev_parse_map_;
	size_t size() const;
public:
	GitObjectCache(std::mutex *mutex = nullptr)
		: mutex_(mutex)
		, object_manager_(mutex)
	{}
	void clear();
	GitHash revParse(GitRunner g, QString const &name);
	GitObject catFile(GitRunner g, const GitHash &id);

	GitHash const &item_id(int i) const
	{
		return items_[i]->id;
	}
};

class GitCommit {
public:
	QString tree_id;
	QStringList parents;

	static bool parseCommit(GitRunner g, GitObjectCache *objcache, GitHash const &id, GitCommit *out);
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
	QString parseCommit(GitRunner g, const GitHash &commit_id);

	GitTreeItemList const *treelist() const
	{
		return &root_item_list;
	}
};

void parseGitTreeObject(QByteArray const &ba, const QString &path_prefix, GitTreeItemList *out);
bool parseGitTreeObject(GitRunner g, GitObjectCache *objcache, QString const &commit_id, QString const &path_prefix, GitTreeItemList *out);

#endif // GITOBJECTMANAGER_H
