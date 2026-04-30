#ifndef GITTYPES_H
#define GITTYPES_H

#include "MyProcess.h"
#include "common/misc.h"
#include "common/qmisc.h"

#include <QDateTime>
// #include <QString>

#define GIT_ID_LENGTH (40)

struct GitRemote {
	std::string name;
	std::string url_fetch;
	std::string url_push;
	std::string ssh_key;
	bool operator < (GitRemote const &r) const
	{
		return name < r.name;
	}
	std::string const &url() const
	{
		return url_fetch;
	}
	void set_url(std::string const &url)
	{
		url_fetch = url;
		url_push = url;
	}
};

class GitHash {
private:
	bool valid_ = false;
	uint8_t id_[GIT_ID_LENGTH / 2];
	template <typename VIEW> void _assign(VIEW const &id);
public:
	GitHash();
	explicit GitHash(std::string_view const &id);
	// explicit GitHash(QString const &id);
	explicit GitHash(char const *id);
	void assign(std::string_view const &id);
	// void assign(const QString &id);
	std::string toString(int maxlen = -1) const;
	bool isValid() const;
	int compare(GitHash const &other) const;
	operator bool () const;
	size_t _std_hash() const;

	static bool isValidID(std::string const &id);
	// static bool isValidID(QString const &id)
	// {
	// 	return isValidID(id.toStdString());
	// }

	static bool isValidID(GitHash const &id)
	{
		return id.isValid();
	}
};

static inline bool operator == (GitHash const &l, GitHash const &r)
{
	return l.compare(r) == 0;
}

static inline bool operator != (GitHash const &l, GitHash const &r)
{
	return l.compare(r) != 0;
}

static inline bool operator < (GitHash const &l, GitHash const &r)
{
	return l.compare(r) < 0;
}

namespace std {
template <> class hash<GitHash> {
public:
	size_t operator () (GitHash const &h) const
	{
		return h._std_hash();
	}
};
}

struct GitObject {
	enum class Type { // 値は固定。packフォーマットで決まってる
		NONE = -1,
		UNKNOWN = 0,
		COMMIT = 1,
		TREE = 2,
		BLOB = 3,
		TAG = 4,
		UNDEFINED = 5,
		OFS_DELTA = 6,
		REF_DELTA = 7,
	};
	Type type = Type::NONE;
	QByteArray content;
	operator bool () const
	{
		return type != Type::NONE;
	}
};

struct GitFileItem {
	std::string name;
	bool isdir = false;
};

struct GitTreeLine {
	int index;
	int depth;
	int color_number = 0;
	bool bend_early = false;
	GitTreeLine(int index = -1, int depth = -1)
		: index(index)
		, depth(depth)
	{
	}
};

struct GitCommitItem {
	GitHash commit_id;
	GitHash tree;
	QList<GitHash> parent_ids;
	std::string author;
	std::string email;
	std::string message;
	QDateTime commit_date;
	std::vector<GitTreeLine> parent_lines;
	bool has_gpgsig = false;
	std::string gpgsig;
	struct {
		std::string text;
		char verify = 0; // git log format:%G?
		std::vector<uint8_t> key_fingerprint;
		std::string trust;
	} sign;
	bool has_child = false;
	int marker_depth = -1;
	bool resolved =  false;
	bool order_fixed = false; // 時差や時計の誤差などの影響により、並び順の調整が行われたとき
	void setParents(const std::vector<std::string> &list);
	operator bool () const
	{
		return commit_id;
	}
	bool operator == (GitCommitItem const &other) const
	{
		return commit_id == other.commit_id;
	}
	bool operator != (GitCommitItem const &other) const
	{
		return commit_id != other.commit_id;
	}
};

class GitCommitItemList {
private:
	std::vector<GitCommitItem> list_;
	std::map<GitHash, size_t> index_;
public:
	void setList(std::vector<GitCommitItem> &&list);
	size_t size() const;
	void resize(size_t n);
	GitCommitItem &at(size_t i);
	GitCommitItem const &at(size_t i) const;
	GitCommitItem &operator [] (size_t i);
	GitCommitItem const &operator [] (size_t i) const;
	void clear();
	bool empty() const;
	void push_front(GitCommitItem const &item);
	QString previousMessage() const;
	void updateIndex();
	int find_index(GitHash const &id) const;
	GitCommitItem *find(GitHash const &id);
	GitCommitItem const *find(GitHash const &id) const;

	void fixCommitLogOrder();
	void updateCommitGraph();
};

class GitResult {
private:
	ProcessStatus status_;
public:
	void set_exit_code(int code)
	{
		status_.exit_code = code;
	}
	void set_output(std::vector<char> const &out)
	{
		status_.output = out;
	}
	void set_error_message(std::string const &msg)
	{
		status_.error_message = msg;
	}

	bool ok() const
	{
		return status_.ok;
	}
	int exit_code()
	{
		return status_.exit_code;
	}
	std::vector<char> const &output() const
	{
		return status_.output;
	}
	std::string output_string() const
	{
		return std::string(status_.output.data(), status_.output.size());
	}
	std::string error_message() const
	{
		return status_.error_message;
	}
	std::string log_message() const
	{
		return status_.log_message;
	}
};

struct GitTag {
	std::string name;
	GitHash id;
};

struct GitUser {
	std::string name;
	std::string email;
	operator bool () const
	{
		return misc::isValidMailAddress(email);
	}
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
	operator bool () const
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

struct GitCloneData {
	QString url;
	QString basedir;
	QString subdir;
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

	void parse(QString const &text);

	GitFileStatus() = default;

	GitFileStatus(QString const &text)
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

struct GitSubmoduleItem {
	std::string name;
	GitHash id;
	std::string path;
	std::string refs;
	std::string url;
	operator bool () const
	{
		return id.isValid() && !path.empty();
	}
};

struct GitSubmoduleUpdateData {
	bool init = true;
	bool recursive = true;
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

class GitHunk {
public:
	std::string at;
	std::vector<std::string> lines;
};

class GitDiff {
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
	GitDiff(std::string const &id, std::string const &path, std::string const &mode);
	bool isSubmodule() const;
private:
	void makeForSingleFile(GitDiff *diff, const std::string &id_a, const std::string &id_b, const std::string &path, const std::string &mode);
};

std::string gitTrimPath(std::string const &s);

#endif // GITTYPES_H
