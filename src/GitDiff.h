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

	void diff_tree_(GitPtr g, QString const &dir, QString older_id, QString newer_id);
	void commit_into_map(GitPtr g, QString const &dir, CommitList const &commit, MapList const *diffmap);
	void parse_tree(GitPtr g, QString const &dir, QString const &id, std::set<QString> *dirset, MapList *path_to_id_map);
	static void AddItem(Git::Diff *item, QList<Git::Diff> *diffs);
public:
	bool diff(GitPtr g, QString id, QList<Git::Diff> *out);
	void interrupt()
	{
		interrupted = true;
	}
public:
	static QString diffFile(GitPtr g, const QString &a_id, const QString &b_id);
	static void parseDiff(const QString &s, const Git::Diff *ref, Git::Diff *out);
	static QString makeKey(const Git::Diff::BLOB_AB &ab);
};


#endif // GITDIFF_H
