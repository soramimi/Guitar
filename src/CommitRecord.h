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
public:
	typedef std::vector<CommitRecord> Vector;
	std::shared_ptr<Vector> records;
	CommitRecords()
		: records(std::make_shared<Vector>())
	{
	}
	void clear()
	{
		records->clear();
	}
	size_t size() const
	{
		return records->size();
	}
	CommitRecord *record(size_t i)
	{
		if (i < size()) {
			return &records->at(i);
		}
		return nullptr;
	}
	CommitRecord const *record(size_t i) const
	{
		return const_cast<CommitRecords *>(this)->record(i);
	}
};

#endif // COMMITRECORD_H
