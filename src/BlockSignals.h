#ifndef BLOCKSIGNALS_H
#define BLOCKSIGNALS_H

#include <QObject>

class BlockSignals {
private:
	QObject *object_;
	bool blocked_ = false;
public:
	BlockSignals(QObject *o)
		: object_(o)
	{
		blocked_ = object_->blockSignals(true);
	}
	~BlockSignals()
	{
		object_->blockSignals(blocked_);
	}
};

#endif // BLOCKSIGNALS_H
