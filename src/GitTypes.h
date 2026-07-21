#ifndef GITTYPES_H
#define GITTYPES_H

#include "GitHash.h"
#include "ProcessStatus.h"
#include <QByteArray>
#include <QList>
#include <map>
#include <span>

#define PATH_PREFIX "*"

struct GitCommitItem;

namespace std {
template <> class hash<GitHash> {
public:
	size_t operator () (GitHash const &h) const
	{
		return h._std_hash();
	}
};
}

struct GitFileItem {
	std::string name;
	bool isdir = false;
};

class GitCommitItemList {
private:
	struct Private;
	Private *m;
	GitCommitItem &_at(size_t i);
	void _update_ptrs();
	void assign(GitCommitItemList const &r);
public:
	GitCommitItemList();
	~GitCommitItemList();
	GitCommitItemList(GitCommitItemList const &r);
	GitCommitItemList(GitCommitItemList &&r);
	GitCommitItemList(std::vector<GitCommitItem> &&list);
	void operator = (GitCommitItemList const &r)
	{
		assign(r);
	}
	void setList(std::vector<GitCommitItem> &&list);
	
	bool empty() const;
	size_t size() const;
	GitCommitItem const &at(size_t i) const;
	GitCommitItem const &operator [] (size_t i) const;
	void push_front(GitCommitItem const &item);
	std::string previousMessage() const;
	void updateIndex();
	
	int find_index(GitHash const &id) const;
	GitCommitItem const *find(GitHash const &id) const;
	
	std::span<GitCommitItem const *const> items() const;

	std::span<GitCommitItem const *const> c_items() const;
	
	void fixCommitLogOrder();
	void updateCommitGraph();
};

struct GitTag {
	std::string name;
	GitHash id;
};

struct GitBranch {
	std::string name;
	GitHash id;
	std::string remote;
	int ahead = 0;
	int behind = 0;
	enum {
		None,
		Current = 0x0001,
		HeadDetachedAt = 0x0002,
		HeadDetachedFrom = 0x0004,
	};
	int flags = 0;
	explicit operator bool () const
	{
		return id.isValid() && !name.empty();
	}
	bool isCurrent() const
	{
		return flags & Current;
	}
	bool isHeadDetached() const
	{
		return flags & HeadDetachedAt;
	}
};

enum class GitSource {
	Default,
	Global,
	Local,
};

class GitFileStatus {
public:
	enum class Code : unsigned int {
		Unknown,
		Ignored,
		Untracked,
		NotUpdated = 0x10000000,
		Staged_ = 0x20000000,
		UpdatedInIndex,
		AddedToIndex,
		DeletedFromIndex,
		RenamedInIndex,
		CopiedInIndex,
		Unmerged_ = 0x40000000,
		Unmerged_BothDeleted,
		Unmerged_AddedByUs,
		Unmerged_DeletedByThem,
		Unmerged_AddedByThem,
		Unmerged_DeletedByUs,
		Unmerged_BothAdded,
		Unmerged_BothModified,
		Tracked_ = 0xf0000000
	};

	struct Data {
		char code_x = 0;
		char code_y = 0;
		Code code = Code::Unknown;
		std::string rawpath1;
		std::string rawpath2;
		std::string path1;
		std::string path2;
	} data;

	static Code parseFileStatusCode(char x, char y);

	bool isStaged() const
	{
		return (int)data.code & (int)Code::Staged_;
	}

	bool isUnmerged() const
	{
		return (int)data.code & (int)Code::Unmerged_;
	}

	bool isTracked() const
	{
		return (int)data.code & (int)Code::Tracked_;
	}

	void parse(std::string const &text);

	GitFileStatus() = default;

	GitFileStatus(std::string const &text)
	{
		parse(text);
	}

	Code code() const
	{
		return data.code;
	}

	int code_x() const
	{
		return data.code_x;
	}

	int code_y() const
	{
		return data.code_y;
	}

	bool isDeleted() const
	{
		return code_x() == 'D' || code_y() == 'D';
	}

	std::string path1() const
	{
		return data.path1;
	}

	std::string path2() const
	{
		return data.path2;
	}

	std::string rawpath1() const
	{
		return data.rawpath1;
	}

	std::string rawpath2() const
	{
		return data.rawpath2;
	}
};

enum class GitMergeFastForward {
	Default,
	No,
	Only,
};

struct GitDiffRaw {
	struct AB {
		std::string id;
		std::string mode;
	} a, b;
	std::string state;
	std::vector<std::string> files;
};

struct GitReflogItem {
	std::string id;
	std::string head;
	std::string command;
	std::string message;
	struct File {
		std::string atts_a;
		std::string atts_b;
		std::string id_a;
		std::string id_b;
		std::string type;
		std::string path;
	};
	QList<File> files;
};

enum GitSignPolicy {
	Unset,
	False,
	True,
};

enum class GitSignatureGrade {
	NoSignature,
	Unknown,
	Good,
	Dubious,
	Missing,
	Bad,
};

std::string gitTrimPath(std::string const &s);

#endif // GITTYPES_H
