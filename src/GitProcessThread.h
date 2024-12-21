#ifndef GITPROCESSTHREAD_H
#define GITPROCESSTHREAD_H

#include "ApplicationGlobal.h"
#include "GitCommandRunner.h"

class GitProcessThread : public QObject {
	Q_OBJECT
public:
	using request_id_t = GitCommandRunner::request_id_t;
private:
	struct Private;
	Private *m;
public:
	GitProcessThread();
	~GitProcessThread();
	void start();
	void stop();
	request_id_t request(GitCommandRunner &&req);
	void cancel(request_id_t reqid);
	bool wait();
};

#endif // GITPROCESSTHREAD_H
