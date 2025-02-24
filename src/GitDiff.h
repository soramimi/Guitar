#ifndef GITDIFF_H
#define GITDIFF_H

#include <set>
#include "common/misc.h"
#include "Git.h"
#include "GitObjectManager.h"

class GitDiff {
	friend class CommitListThread;
private:
	class LookupTable;
private:
	GitObjectCache *objcache = nullptr;
	QList<Git::Diff> diffs;

	using MapList = std::list<LookupTable>;

        GitRunner git(const Git::SubmoduleItem &submod);

	static void AddItem(Git::Diff *item, QList<Git::Diff> *diffs);

	void retrieveCompleteTree(GitRunner g, QString const &dir, GitTreeItemList const *files, std::map<QString, GitTreeItem> *out);
        void retrieveCompleteTree(GitRunner g, QString const &dir, GitTreeItemList const *files);
public:
	GitDiff(GitObjectCache *objcache)
	{
		this->objcache = objcache;
	}

        bool diff(GitRunner g, const Git::Hash &id, const QList<Git::SubmoduleItem> &submodules, QList<Git::Diff> *out);
        bool diff_uncommited(GitRunner g, const QList<Git::SubmoduleItem> &submodules, QList<Git::Diff> *out);

public:
        static QString diffObjects(GitRunner g, QString const &a_id, QString const &b_id);
		static QString diffFiles(GitRunner g, QString const &a_path, QString const &b_path);
	static void parseDiff(std::string const &s, const Git::Diff *info, Git::Diff *out);
	static QString makeKey(const QString &a_id, const QString &b_id);
	static QString makeKey(const Git::Diff &diff);
	static QString prependPathPrefix(QString const &path);
};

QString lookupFileID(GitRunner g, GitObjectCache *objcache, QString const &commit_id, QString const &file);

#endif // GITDIFF_H
