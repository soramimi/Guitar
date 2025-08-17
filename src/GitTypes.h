#ifndef GITTYPES_H
#define GITTYPES_H

#include "MyProcess.h"
#include "common/misc.h"
#include <QDateTime>
#include <QString>

#define GIT_ID_LENGTH (40)
#define PATH_PREFIX "*"

struct GitRemote {
	QString name;
	QString url_fetch;
	QString url_push;
	QString ssh_key;
	bool operator < (GitRemote const &r) const
	{
		return name < r.name;
	}
	QString const &url() const
	{
		return url_fetch;
	}
	void set_url(QString const &url)
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
	explicit GitHash(QString const &id);
	explicit GitHash(char const *id);
	void assign(std::string_view const &id);
	void assign(const QString &id);
	QString toQString(int maxlen = -1) const;
	bool isValid() const;
	int compare(GitHash const &other) const;
	operator bool () const;
	size_t _std_hash() const;

	static bool isValidID(QString const &id);

	static bool isValidID(GitHash const &id)
	{
		return id.isValid();
	}
};

static inline bool operator == (GitHash const &l, GitHash const &r)
{
	return l.compare(r) == 0;
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
	QString name;
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
	QString author;
	QString email;
	QString message;
	QDateTime commit_date;
	std::vector<GitTreeLine> parent_lines;
	bool has_gpgsig = false;
	QString gpgsig;
	struct {
		QString text;
		char verify = 0; // git log format:%G?
		std::vector<uint8_t> key_fingerprint;
		QString trust;
	} sign;
	bool has_child = false;
	int marker_depth = -1;
	bool resolved =  false;
	bool order_fixed = false; // 時差や時計の誤差などの影響により、並び順の調整が行われたとき
	void setParents(QStringList const &list);
	operator bool () const
	{
		return commit_id;
	}
};

class GitCommitItemList {
public:
	std::vector<GitCommitItem> list;
	std::map<GitHash, size_t> index;
	size_t size() const
	{
		return list.size();
	}
	void resize(size_t n)
	{
		list.resize(n);
	}
	GitCommitItem &at(size_t i)
	{
		return list[i];
	}
	GitCommitItem const &at(size_t i) const
	{
		return list.at(i);
	}
	GitCommitItem &operator [] (size_t i)
	{
		return at(i);
	}
	GitCommitItem const &operator [] (size_t i) const
	{
		return at(i);
	}
	void clear()
	{
		list.clear();
	}
	bool empty() const
	{
		return list.empty();
	}
	void updateIndex()
	{
		index.clear();
		for (size_t i = 0; i < list.size(); i++) {
			index[list[i].commit_id] = i;
		}
	}
	GitCommitItem *find(GitHash const &id)
	{
		auto it = index.find(id);
		if (it != index.end()) {
			return &list[it->second];
		}
		return nullptr;
	}
	GitCommitItem const *find(GitHash const &id) const
	{
		return const_cast<GitCommitItemList *>(this)->find(id);
	}
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
	QString name;
	GitHash id;
};

struct GitUser {
	QString name;
	QString email;
	operator bool () const
	{
		return misc::isValidMailAddress(email);
	}
};

struct GitBranch {
	QString name;
	GitHash id;
	QString remote;
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
		return id.isValid() && !name.isEmpty();
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
		QString rawpath1;
		QString rawpath2;
		QString path1;
		QString path2;
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

	QString path1() const
	{
		return data.path1;
	}

	QString path2() const
	{
		return data.path2;
	}

	QString rawpath1() const
	{
		return data.rawpath1;
	}

	QString rawpath2() const
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
	QString name;
	GitHash id;
	QString path;
	QString refs;
	QString url;
	operator bool () const
	{
		return id.isValid() && !path.isEmpty();
	}
};

struct GitSubmoduleUpdateData {
	bool init = true;
	bool recursive = true;
};

struct GitDiffRaw {
	struct AB {
		QString id;
		QString mode;
	} a, b;
	QString state;
	QStringList files;
};

struct GitReflogItem {
	QString id;
	QString head;
	QString command;
	QString message;
	struct File {
		QString atts_a;
		QString atts_b;
		QString id_a;
		QString id_b;
		QString type;
		QString path;
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
	QString diff;
	QString index;
	QString path;
	QString mode;
	struct BLOB_AB_ {
		QString a_id_or_path; // コミットIDまたはファイルパス。パスのときは PATH_PREFIX（'*'）で始まる
		QString b_id_or_path;
	} blob;
	QList<GitHunk> hunks;
	struct SubmoduleDetail {
		GitSubmoduleItem item;
		GitCommitItem commit;
	} a_submodule, b_submodule;
	GitDiff() = default;
	GitDiff(QString const &id, QString const &path, QString const &mode);
	bool isSubmodule() const;
private:
	void makeForSingleFile(GitDiff *diff, QString const &id_a, QString const &id_b, QString const &path, QString const &mode);
};

QString gitTrimPath(QString const &s);

#endif // GITTYPES_H
