#ifndef COMMITDETAILGETTER_H
#define COMMITDETAILGETTER_H

#include "Git.h"
#include <QObject>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

class CommitDetailGetter : public QObject {
	Q_OBJECT
private:
	std::mutex mutex_;
	std::condition_variable condition_;
	std::shared_ptr<std::thread> thread_;
	bool interrupted_ = false;

	struct Item  {
		bool done = false;
		bool busy = false;
		Git::CommitID id;
		int value = 0;
		operator bool () const
		{
			return done;
		}
	};
	std::vector<Item> requests_;

	GitPtr git_;

public:
	CommitDetailGetter() = default;
	virtual ~CommitDetailGetter();
	void start(GitPtr git);
	void stop();
	CommitDetailGetter::Item request(Git::CommitID id);
	void apply(Git::CommitItemList *logs);
signals:
	void ready();
};

#endif // COMMITDETAILGETTER_H
