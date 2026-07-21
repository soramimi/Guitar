
#ifndef GITDIFF_H
#define GITDIFF_H

#include "GitCommitItem.h"
#include "GitSubmodule.h"
#include <QList>
#include <string>

class GitHunk {
public:
	std::string at;
	std::vector<std::string> lines;
};

class GitDiff {
private:
	void makeForSingleFile(GitDiff *diff, const std::string &id_a, const std::string &id_b, const std::string &path, const std::string &mode);
public:
	enum class Type {
		Unknown,
		Modify,
		Copy,
		Rename,
		Create,
		Delete,
		ChType,
		Unmerged,
	};
	Type type = Type::Unknown;
	std::string diff;
	std::string index;
	std::string path;
	std::string mode;
	struct BLOB_AB_ {
		std::string a_id_or_path; // コミットIDまたはファイルパス。パスのときは PATH_PREFIX（'*'）で始まる
		std::string b_id_or_path;
	} blob;
	QList<GitHunk> hunks;
	struct SubmoduleDetail {
		GitSubmoduleItem item;
		GitCommitItem commit;
	} a_submodule, b_submodule;
	GitDiff() = default;
	GitDiff(std::string const &id, std::string const &path, std::string const &mode)
	{
		makeForSingleFile(this, std::string(GIT_ID_LENGTH, '0'), id, path, mode);
	}
	bool isSubmodule() const
	{
		return mode == "160000";
	}
};

struct GitDiffOption {
	bool ignore_space_change = false;
};



#endif // GITDIFF_H
