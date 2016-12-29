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

	void diff_tree_(Git *g, QString const &dir, QString older_index, QString newer_index);
	void commit_into_map(Git *g, QString const &dir, CommitList const &commit, MapList const *diffmap);
	void file_into_map(Git::FileStatusList const &stats, MapList const *diffmap);
	void parse_commit(Git *g, QString const &dir, QString const &index, MapList *diffmaplist);
	void parse_tree(Git *g, QString const &dir, QString const &index, std::set<QString> *dirset, MapList *diffmaplist);
public:
	void diff(GitPtr g, QString index, QList<Git::Diff> *out);
};


#endif // GITDIFF_H
