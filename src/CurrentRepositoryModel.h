#ifndef CURRENTREPOSITORYMODEL_H
#define CURRENTREPOSITORYMODEL_H

#include "BranchLabel.h"
#include "GitObjectManager.h"
#include "RepositoryData.h"
#include <optional>
#include <mutex>
#include <map>
#include <memory>

class CurrentRepositoryModel {
public:
	RepositoryData repository_data;

	Git::CommitItemList commit_log;
	std::map<Git::CommitID, QList<Git::Branch>> branch_map;
	std::map<Git::CommitID, QList<Git::Tag>> tag_map;
	std::map<int, QList<BranchLabel>> label_map;

	std::map<QString, Git::Diff> diff_cache;
	GitObjectCache object_cache;

public:

	void setCurrentLogRow(int row);
};

struct CommitLogExchangeData {
	struct D {
		std::optional<Git::CommitItemList> commit_log;
		std::optional<std::map<Git::CommitID, QList<Git::Branch>>> branch_map;
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

#endif // CURRENTREPOSITORYMODEL_H
