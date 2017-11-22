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
//	GitPtr g;
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

	typedef std::list<LookupTable> MapList;

	GitPtr git();

	static void AddItem(Git::Diff *item, QList<Git::Diff> *diffs);

	void retrieveCompleteTree(const QString &dir, const GitTreeItemList *files, std::map<QString, GitTreeItem> *out);
	void retrieveCompleteTree(const QString &dir, const GitTreeItemList *files);
public:
	GitDiff(GitObjectCache *objcache)
	{
		this->objcache = objcache;
	}

	bool diff(QString id, QList<Git::Diff> *out);
	bool diff_uncommited(QList<Git::Diff> *out);

	void interrupt()
	{
		interrupted = true;
	}

public:
	static QString diffFile(GitPtr g, const QString &a_id, const QString &b_id);
	static void parseDiff(std::string const &s, const Git::Diff *info, Git::Diff *out);
	static QString makeKey(QString const &a_id, QString const &b_id);
	static QString makeKey(const Git::Diff &diff);
	static QString prependPathPrefix(const QString &path);


};

#endif // GITDIFF_H
