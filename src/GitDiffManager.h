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
public:
	GitDiffManager(GitObjectCache *objcache)
	{
		objcache_ = objcache;
	}

	QList<GitDiff> diff(GitRunner g, const GitHash &id, const QList<GitSubmoduleItem> &submodules);
	QList<GitDiff> diff_uncommited(GitRunner g, const QList<GitSubmoduleItem> &submodules);

public:
	static std::string diffObjects(GitRunner g, const std::string &a_id, const std::string &b_id);
	static std::string diffFiles(GitRunner g, const std::string &a_path, const std::string &b_path);
	static GitDiff parseDiff(std::string const &s, const GitDiff *info);
	static QString makeKey(const QString &a_id, const QString &b_id);
	static QString makeKey(const GitDiff &diff);
	static QString prependPathPrefix(QString const &path);
};

QString lookupFileID(GitRunner g, GitObjectCache *objcache, const GitHash &commit_id, QString const &file);

#endif // GITDIFFMANAGER_H
