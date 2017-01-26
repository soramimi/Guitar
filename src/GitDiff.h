#ifndef GITDIFF_H
#define GITDIFF_H

#include <set>
#include "misc.h"
#include "Git.h"

struct TreeItem;

typedef QList<TreeItem> TreeItemList;

class GitDiff {
	friend class CommitListThread;
public:
private:
	class LookupTable;
private:
	GitPtr g;
	GitObjectCache *objcache = nullptr;
	QList<Git::Diff> diffs;

	bool interrupted = false;

	struct Interrupted {
	};

	void checkInterrupted()
	{
		if (interrupted) {
			throw Interrupted();
		}
	}

	typedef std::list<LookupTable> MapList;

	void diff_tree_(GitPtr g, QString const &dir, QString older_id, QString newer_id);
	void commit_into_map(GitPtr g, const TreeItemList *files, MapList const *diffmap);
	void parseTree_(GitPtr g, GitObjectCache *objcache, QString const &dir, QString const &id, std::set<QString> *dirset, MapList *path_to_id_map);
	static void AddItem(Git::Diff *item, QList<Git::Diff> *diffs);

	void retrieveCompleteTree(const QString &dir, TreeItemList *files);
public:
	GitDiff(GitPtr g, GitObjectCache *objcache)
	{
		this->g = g;
		this->objcache = objcache;
	}

	bool diff(QString id, QList<Git::Diff> *out, bool uncommited);
	void interrupt()
	{
		interrupted = true;
	}

public:
	static QString diffFile(GitPtr g, const QString &a_id, const QString &b_id);
	static void parseDiff(const QString &s, const Git::Diff *info, Git::Diff *out);
	static QString makeKey(QString const &a_id, QString const &b_id);
	static QString makeKey(const Git::Diff &diff);
	static QString prependPathPrefix(const QString &path);


};

QString lookupFileID(GitPtr g, GitObjectCache *objcache, const QString &commit_id, const QString &file);


#endif // GITDIFF_H
