#ifndef GITDIFFMANAGER_H
#define GITDIFFMANAGER_H

#include "GitObjectManager.h"

class GitDiffManager {
	friend class CommitListThread;
private:
	class LookupTable;
private:
	GitObjectCache *objcache_ = nullptr;

	using MapList = std::list<LookupTable>;

	GitRunner git_for_submodule(GitRunner g, const GitSubmoduleItem &submod);

	static void AddItem(GitDiff *item, QList<GitDiff> *diffs);

	void retrieveCompleteTree(GitRunner g, QString const &dir, GitTreeItemList const *files, QList<GitDiff> *diffs);
public:
	GitDiffManager(GitObjectCache *objcache)
	{
		objcache_ = objcache;
	}

	QList<GitDiff> diff(GitRunner g, const GitHash &id, const QList<GitSubmoduleItem> &submodules);
	QList<GitDiff> diff_uncommited(GitRunner g, const QList<GitSubmoduleItem> &submodules);

public:
	static QString diffObjects(GitRunner g, QString const &a_id, QString const &b_id);
	static QString diffFiles(GitRunner g, QString const &a_path, QString const &b_path);
	static GitDiff parseDiff(std::string const &s, const GitDiff *info);
	static QString makeKey(const QString &a_id, const QString &b_id);
	static QString makeKey(const GitDiff &diff);
	static QString prependPathPrefix(QString const &path);
};

QString lookupFileID(GitRunner g, GitObjectCache *objcache, const GitHash &commit_id, QString const &file);

#endif // GITDIFFMANAGER_H
