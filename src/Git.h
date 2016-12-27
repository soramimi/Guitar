#ifndef GIT_H
#define GIT_H

#include <QDateTime>
#include <QObject>
#include <functional>

#include <memory>

#define USE_LIBGIT2 0

enum class LineSide {
	Left,
	Right,
};

struct TreeLine {
	int index;
	int depth;
	int color_number = 0;
	bool bend_early = false;
	TreeLine(int index = -1, int depth = -1)
		: index(index)
		, depth(depth)
	{
	}
};

class Git : QObject {
public:
	class Context {
	public:
		QString git_command;
	};
	class Hunk {
	public:
		QString at;
		QStringList lines;
	};
	struct BLOB {
		QString id;
		QString path;
	};
	class Diff {
	public:
		QString diff;
		QString index;
		QString path;
		QString mode;
		struct BLOB_AB {
			BLOB a;
			BLOB b;
		} blob;
		QList<Hunk> hunks;
	};

	struct CommitItem {
		QString commit_id;
		QStringList parent_ids;
		QString author;
		QString mail;
		QString message;
		QDateTime commit_date;
		std::vector<TreeLine> parent_lines;
		bool has_child = false;
		int marker_depth = -1;
		bool resolved =  false;

	};
	typedef std::vector<CommitItem> CommitItemList;

	static bool isUncommited(CommitItem const &item)
	{
		return item.commit_id.isEmpty();
	}

	struct Branch {
		QString name;
		QString id;
		int ahead = 0;
		int behind = 0;
		enum {
			None,
			Current = 0x0001,
		};
		int flags = 0;
	};

	enum class FileStatusCode : unsigned int {
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

	class FileStatus {
	public:
		struct Data {
			char code_x = 0;
			char code_y = 0;
			FileStatusCode code = FileStatusCode::Unknown;
			QString path1;
			QString path2;
		} data;

		static FileStatusCode parseFileStatusCode(char x, char y)
		{
			if (x == ' ' && (y == 'M' || y == 'D')) return FileStatusCode::NotUpdated;
			if (x == 'M' && (y == 'M' || y == 'D' || y == ' ')) return FileStatusCode::UpdatedInIndex;
			if (x == 'A' && (y == 'M' || y == 'D' || y == ' ')) return FileStatusCode::AddedToIndex;
			if (x == 'D' && (y == 'M' || y == ' ')) return FileStatusCode::DeletedFromIndex;
			if (x == 'R' && (y == 'M' || y == 'D' || y == ' ')) return FileStatusCode::RenamedInIndex;
			if (x == 'C' && (y == 'M' || y == 'D' || y == ' ')) return FileStatusCode::CopiedInIndex;
			if (x == 'C' && (y == 'M' || y == 'D' || y == ' ')) return FileStatusCode::CopiedInIndex;
			if (x == 'D' && y == 'D') return FileStatusCode::Unmerged_BothDeleted;
			if (x == 'A' && y == 'U') return FileStatusCode::Unmerged_AddedByUs;
			if (x == 'U' && y == 'D') return FileStatusCode::Unmerged_DeletedByThem;
			if (x == 'U' && y == 'A') return FileStatusCode::Unmerged_AddedByThem;
			if (x == 'D' && y == 'U') return FileStatusCode::Unmerged_DeletedByUs;
			if (x == 'A' && y == 'A') return FileStatusCode::Unmerged_BothAdded;
			if (x == 'U' && y == 'U') return FileStatusCode::Unmerged_BothModified;
			if (x == '?' && y == '?') return FileStatusCode::Untracked;
			if (x == '!' && y == '!') return FileStatusCode::Ignored;
			return FileStatusCode::Unknown;
		}

		bool isStaged() const
		{
			return (int)data.code & (int)FileStatusCode::Staged_;
		}

		bool isUnmerged() const
		{
			return (int)data.code & (int)FileStatusCode::Unmerged_;
		}

		bool isTracked() const
		{
			return (int)data.code & (int)FileStatusCode::Tracked_;
		}

		void parse(QString const &text);

		FileStatus()
		{
		}

		FileStatus(QString const &text)
		{
			parse(text);
		}

		FileStatusCode code() const
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

		QString path1() const
		{
			return data.path1;
		}

		QString path2() const
		{
			return data.path2;
		}
	};
	typedef std::vector<FileStatus> FileStatusList;

	static QString trimPath(const QString &s);

private:
	struct Private;
	Private *pv;
	QStringList make_branch_list_();
	QByteArray cat_file_(const QString &id);
	FileStatusList status_();
	bool commit_(const QString &msg, bool amend);
	void push_();
#if USE_LIBGIT2
	QString diffHeadToWorkingDir_();
	QString diff_(const QString &old_id, const QString &new_id);
#endif
	static void parseAheadBehind(const QString &s, Branch *b);
public:
	Git(Context const &cx, const QString &repodir);
	Git(Git &&r)
	{
		pv = r.pv;
		r.pv = nullptr;
	}
	virtual ~Git();
	QByteArray result() const;
	void setGitCommand(const QString &path);
	QString gitCommand() const;
	void clearResult();
	QString resultText() const;
	bool chdirexec(std::function<bool ()> fn);
	bool git(QString const &arg, bool chdir = true);

	void setWorkingRepositoryDir(const QString &repo);
	const QString &workingRepositoryDir() const;

	QString getCurrentBranchName();
	bool isValidWorkingCopy();
	QString version();
	QString revparseHEAD();
	QStringList getUntrackedFiles();
	CommitItemList log_all(const QString &id, int maxcount, QDateTime limit_time);
	CommitItemList log(int maxcount, QDateTime limit_time);

	bool clone(const QString &location, const QString &path);

	FileStatusList status();
	bool cat_file(const QString &id, QByteArray *out);
	void revertFile(const QString &path);
	void revertAllFiles();

	void removeFile(const QString &path);

	void stage(const QString &path);
	void stage(const QStringList &paths);
	void unstage(const QString &path);
	void unstage(const QStringList &paths);
	void pull();

	void fetch();

	QList<Branch> branches_();
	QList<Branch> branches();

	int getProcessExitCode() const;

	QString diff(QString const &old_id, QString const &new_id);

	static bool isValidID(QString const &s);

	bool commit(const QString &text);
	bool commit_amend_m(const QString &text);
	void push();
	void test();
	void createBranch(const QString &name);
	void checkoutBranch(const QString &name);
	void mergeBranch(const QString &name);
	static bool isValidWorkingCopy(const QString &dir);
	QString diff_to_file(const QString &old_id, const QString &path);
	QString errorMessage() const;
};


#endif // GIT_H
