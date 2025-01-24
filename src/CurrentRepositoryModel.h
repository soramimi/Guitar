#ifndef CURRENTREPOSITORYMODEL_H
#define CURRENTREPOSITORYMODEL_H

#include "BranchLabel.h"
#include "GitObjectManager.h"
#include "RepositoryData.h"
#include <optional>
#include <mutex>
#include <map>

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
	std::optional<Git::CommitItemList> commit_log;
	std::optional<std::map<Git::CommitID, QList<Git::Branch>>> branch_map;
};
Q_DECLARE_METATYPE(CommitLogExchangeData)

#endif // CURRENTREPOSITORYMODEL_H
