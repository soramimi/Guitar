
#ifndef GITCOMMITITEM_H
#define GITCOMMITITEM_H

#include "GitHash.h"
#include <q/DateTime.h>
#include <vector>

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
	std::vector<GitHash> parent_ids;
	std::string author;
	std::string email;
	std::string message;
	DateTime commit_date;
	std::vector<GitTreeLine> parent_lines;
	bool has_gpgsig = false;
	std::string gpgsig;
	struct {
		std::string text;
		char verify = 0; // git log format:%G?
		std::vector<uint8_t> key_fingerprint;
		std::string trust;
	} sign;
	bool has_child = false;
	int marker_depth = -1;
	bool resolved =  false;
	// bool order_fixed = false; // 時差や時計の誤差などの影響により、並び順の調整が行われたとき
	void setParents(const std::vector<std::string> &list);
	explicit operator bool () const
	{
		return (bool)commit_id;
	}
	bool operator == (GitCommitItem const &other) const
	{
		return commit_id == other.commit_id;
	}
	bool operator != (GitCommitItem const &other) const
	{
		return commit_id != other.commit_id;
	}
};

#endif // GITCOMMITITEM_H
