#ifndef GITDIFF_H
#define GITDIFF_H

#include <set>
#include "common/misc.h"
#include "Git.h"
#include "GitObjectManager.h"

class GitDiff {
	friend class CommitListThread;
public:
private:
	class LookupTable;
private:
	GitObjectCache *objcache = nullptr;
	QList<Git::Diff> diffs;

	bool interrupted = false;

	struct Interrupted {
	};

	void checkInterrupted()
	{
		if (interrupted) {
			throw Interrupted();
		}
	}

	using MapList = std::list<LookupTable>;

	static GitPtr git(const Git::SubmoduleItem &submod);

	static void AddItem(Git::Diff *item, QList<Git::Diff> *diffs);

	void retrieveCompleteTree(GitPtr g, QString const &dir, GitTreeItemList const *files, std::map<QString, GitTreeItem> *out);
	void retrieveCompleteTree(GitPtr g, QString const &dir, GitTreeItemList const *files);
public:
	GitDiff(GitObjectCache *objcache)
	{
		this->objcache = objcache;
	}

	bool diff(GitPtr g, const Git::CommitID &id, const QList<Git::SubmoduleItem> &submodules, QList<Git::Diff> *out);
	bool diff_uncommited(GitPtr g, const QList<Git::SubmoduleItem> &submodules, QList<Git::Diff> *out);

	void interrupt()
	{
		interrupted = true;
	}

public:
	static QString diffObjects(GitPtr g, QString const &a_id, QString const &b_id);
	static QString diffFiles(GitPtr g, QString const &a_path, QString const &b_path);
	static void parseDiff(std::string const &s, const Git::Diff *info, Git::Diff *out);
	static QString makeKey(const QString &a_id, const QString &b_id);
	static QString makeKey(const Git::Diff &diff);
	static QString prependPathPrefix(QString const &path);
};

QString lookupFileID(GitPtr g, GitObjectCache *objcache, QString const &commit_id, QString const &file);

#endif // GITDIFF_H
