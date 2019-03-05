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

	GitPtr git();

	static void AddItem(Git::Diff *item, QList<Git::Diff> *diffs);

	void retrieveCompleteTree(QString const &dir, GitTreeItemList const *files, std::map<QString, GitTreeItem> *out);
	void retrieveCompleteTree(QString const &dir, GitTreeItemList const *files);
public:
	GitDiff(GitObjectCache *objcache)
	{
		this->objcache = objcache;
	}

	bool diff(QString const &id, QList<Git::Diff> *out);
	bool diff_uncommited(QList<Git::Diff> *out);

	void interrupt()
	{
		interrupted = true;
	}

public:
	static QString diffObjects(const GitPtr &g, QString const &a_id, QString const &b_id);
	static QString diffFiles(const GitPtr &g, QString const &a_path, QString const &b_path);
	static void parseDiff(std::string const &s, const Git::Diff *info, Git::Diff *out);
	static QString makeKey(QString const &a_id, QString const &b_id);
	static QString makeKey(const Git::Diff &diff);
	static QString prependPathPrefix(QString const &path);


};

#endif // GITDIFF_H
