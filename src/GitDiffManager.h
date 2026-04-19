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

	std::vector<GitDiff> diff(GitRunner g, const GitHash &id, const std::vector<GitSubmoduleItem> &submodules);
	std::vector<GitDiff> diff_uncommited(GitRunner g, const std::vector<GitSubmoduleItem> &submodules);

public:
	static std::string diffObjects(GitRunner g, const std::string &a_id, const std::string &b_id);
	static std::string diffFiles(GitRunner g, const std::string &a_path, const std::string &b_path);
	static GitDiff parseDiff(std::string const &s, const GitDiff *info);
	static std::string makeKey(std::string const &a_id, std::string const &b_id);
	static std::string makeKey(const GitDiff &diff);
	static std::string prependPathPrefix(const std::string &path);
};

QString lookupFileID(GitRunner g, GitObjectCache *objcache, const GitHash &commit_id, QString const &file);

#endif // GITDIFFMANAGER_H
