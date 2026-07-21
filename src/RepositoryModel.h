#ifndef REPOSITORYMODEL_H
#define REPOSITORYMODEL_H

#include "BranchLabel.h"
#include "GitRunner.h"
#include <QMetaType>

typedef std::vector<GitTag> TagList;
typedef QList<GitBranch> BranchList;
typedef QList<BranchLabel> BranchLabelList;

struct RepositoryData {
	GitCommitItemList commit_log;
	std::map<GitHash, BranchList> branch_map;
	std::map<GitHash, TagList> tag_map;
	std::map<int, BranchLabelList> label_map;
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
		: p(r.p)
	{}
};
Q_DECLARE_METATYPE(CommitLogExchangeData)

#endif // REPOSITORYMODEL_H
