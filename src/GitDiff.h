#ifndef GITDIFF_H
#define GITDIFF_H



#include <set>
#include "misc.h"
#include "Git.h"

class GitDiff {
public:
private:
	struct CommitData;
	class CommitList;
	class IndexMap;
private:
	std::vector<Git::Diff> diffs;


	typedef std::list<IndexMap> MapList;

	static void diff_tree_(GitPtr g, QString const &dir, QString older_index, QString newer_index, std::vector<Git::Diff> *diffs);
	static void commit_into_map(GitPtr g, QString const &dir, CommitList const &commit, MapList const *diffmap, std::vector<Git::Diff> *diffs);
	static void parse_commit(GitPtr g, QString const &dir, QString const &index, MapList *diffmaplist);
	static void parse_tree(GitPtr g, QString const &dir, QString const &index, std::set<QString> *dirset, MapList *diffmaplist);
	static void file_into_map(Git::FileStatusList const &stats, MapList const *diffmap, std::vector<Git::Diff> *diffs);
public:
	void diff(GitPtr g, QString index, QList<Git::Diff> *out);
};


#endif // GITDIFF_H
