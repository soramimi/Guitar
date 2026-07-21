
#ifndef COMMITDETAILGETTER_H
#define COMMITDETAILGETTER_H

#include "GitHash.h"
#include <QObject>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

class GitRunner;

class CommitDetailGetter : public QObject {
	Q_OBJECT
public:
	struct Data {
		char sign_verify = 0;
	};
private:
	static constexpr int num_threads = 1;

	struct Private;
	Private *m;


	struct Request {
		bool done = false;
		bool busy = false;
		GitHash id;
		Data data;

		explicit operator bool () const
		{
			return done;
		}
	};
public:
	CommitDetailGetter();
	virtual ~CommitDetailGetter();
	void start(GitRunner const &git);
	void stop();
private:
	Data _query(const GitHash &id, bool request_if_not_found, bool lock);
public:
	Data query(const GitHash &id, bool request_if_not_found);
signals:
	void ready();
};

#endif // COMMITDETAILGETTER_H
