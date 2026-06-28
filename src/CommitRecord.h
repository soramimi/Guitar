#ifndef COMMITRECORD_H
#define COMMITRECORD_H

#include "GitTypes.h"

struct CommitRecord {
	bool bold = false;
	GitHash commit_hash;
	QString datetime;
	QString author;
	QString message;
	QString tooltip;
	std::string commit_id() const
	{
		return commit_hash.toString();
	}
};
Q_DECLARE_METATYPE(CommitRecord)

class CommitRecords {
private:
	std::vector<CommitRecord> records_;
	std::vector<CommitRecord const *> records_ptrs_;
public:
	void clear()
	{
		records_.clear();
	}
	void setRecords(std::vector<CommitRecord> &&v)
	{
		records_ = v;
		const auto N = records_.size();
		records_ptrs_.resize(N);
		for (size_t i = 0; i < N; i++) {
			records_ptrs_[i] = &records_[i];
		}
	}
	std::basic_string_view<CommitRecord const *> records() const
	{
		return {records_ptrs_.data(), records_ptrs_.size()};
	}
};

#endif // COMMITRECORD_H
