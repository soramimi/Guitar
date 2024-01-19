#ifndef COMMITDETAILGETTER_H
#define COMMITDETAILGETTER_H

#include "Git.h"
#include <QObject>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <map>

class CommitDetailGetter : public QObject {
	Q_OBJECT
public:
	struct Data {
		char sign_verify = 0;
	};
private:
	std::mutex mutex_;
	std::condition_variable condition_;
	std::vector<std::thread> threads_;
	bool interrupted_ = false;

	GitPtr git_;

	struct Request {
		bool done = false;
		bool busy = false;
		Git::CommitID id;
		Data data;

		operator bool () const
		{
			return done;
		}
	};
	std::vector<Request> requests_;
	std::map<Git::CommitID, Data> cache_;
public:
	CommitDetailGetter() = default;
	virtual ~CommitDetailGetter();
	void start(GitPtr git);
	void stop();
	Data query(const Git::CommitID &id, bool request_if_not_found, bool lock);
signals:
	void ready();
};

#endif // COMMITDETAILGETTER_H
