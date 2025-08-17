#ifndef REPOSITORYMODEL_H
#define REPOSITORYMODEL_H

#include "Git.h"
#include "BranchLabel.h"
#include "GitCommandCache.h"
#include "GitObjectManager.h"

typedef QList<GitTag> TagList;
typedef QList<GitBranch> BranchList;
typedef QList<BranchLabel> BranchLabelList;

struct RepositoryData {
	std::mutex *mutex_ = nullptr;

	GitCommitItemList commit_log;
	std::map<GitHash, BranchList> branch_map;
	std::map<GitHash, TagList> tag_map;
	std::map<int, BranchLabelList> label_map;

	std::map<QString, GitDiff> diff_cache;
	GitObjectCache object_cache;

	GitCommandCache git_command_cache;

	RepositoryData(std::mutex *mutex)
		: mutex_(mutex)
		, object_cache(mutex)
	{}
};

struct CommitLogExchangeData {
	struct D {
		std::optional<GitCommitItemList> commit_log;
		std::optional<std::map<GitHash, BranchList>> branch_map;
		std::optional<std::map<GitHash, TagList>> tag_map;
		std::optional<std::map<int, BranchLabelList>> label_map;
	};
	std::shared_ptr<D> p;
	CommitLogExchangeData()
		: p(std::make_shared<D>())
	{}
	CommitLogExchangeData(CommitLogExchangeData const &r)
		: p(std::make_shared<D>(*r.p))
	{}
};
Q_DECLARE_METATYPE(CommitLogExchangeData)

#endif // REPOSITORYMODEL_H
