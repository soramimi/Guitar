#ifndef GITDIFF_H
#define GITDIFF_H



#include <set>
#include "misc.h"
#include "Git.h"

class GitDiff {
	friend class CommitListThread;
public:
private:
	class CommitList;
	class LookupTable;
private:
	QList<Git::Diff> diffs;


	typedef std::list<LookupTable> MapList;

	static void diff_tree_(GitPtr g, QString const &dir, QString older_id, QString newer_id, QList<Git::Diff> *diffs);
	static void commit_into_map(GitPtr g, QString const &dir, CommitList const &commit, MapList const *diffmap, QList<Git::Diff> *diffs);
	static void parse_tree(GitPtr g, QString const &dir, QString const &id, std::set<QString> *dirset, MapList *path_to_id_map);
	static void AddItem(Git::Diff *item, QList<Git::Diff> *diffs);
public:
	void diff(GitPtr g, QString id, QList<Git::Diff> *out);
	static QString diffFile(GitPtr g, const QString &a_id, const QString &b_id);
	static void parseDiff(const QString &s, const Git::Diff *ref, Git::Diff *out);
	static QString makeKey(const Git::Diff::BLOB_AB &ab);
};


#endif // GITDIFF_H
