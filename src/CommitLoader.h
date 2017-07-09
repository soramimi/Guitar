#ifndef COMMITLOADER_H
#define COMMITLOADER_H

#include "Git.h"
#include "GitHubAPI.h"
#include "webclient.h"
#include <QIcon>
#include <QThread>
#include <QMutex>
#include <deque>
#include <QWaitCondition>
#include <set>
#include <string>

class CommitLoader : public QThread {
	Q_OBJECT
public:
	struct RequestItem {
		std::string url;
		GitHubAPI::User user;
	};
private:
	GitHubAPI github;

	struct Private;
	Private *m;

	QString makeGitHubCommitQuery(const Git::CommitItem *commit);
protected:
	void run();
public:
	CommitLoader();
	~CommitLoader();
	GitHubAPI::User fetch(const QString &url, bool request);
	std::deque<RequestItem> takeResults();
	void interrupt();
	void start(WebContext *webcx);
signals:
	void updated();
};

#endif // COMMITLOADER_H
