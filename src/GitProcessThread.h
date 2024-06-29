#ifndef GITPROCESSTHREAD_H
#define GITPROCESSTHREAD_H

#include "ApplicationGlobal.h"
#include "Git.h"
#include "MyProcess.h"
#include <QMetaType>
#include <QObject>
#include <QString>
#include <functional>

class AbstractGitCommandItem {
public:
	GitPtr g;
	PtyProcess *pty = nullptr;
	QString progress_message;
	bool update_commit_log = false;
	std::vector<std::function<void (AbstractGitCommandItem const *)>> done;
	bool reopen_repository = true;
	AbstractGitCommandItem(QString const &progress_message)
		: progress_message(progress_message)
	{
	}
	virtual ~AbstractGitCommandItem() = default;
	virtual void run() = 0;
};

class GitCommandItem_fetch : public AbstractGitCommandItem {
public:
	bool prune;
	GitCommandItem_fetch(QString const &progress_message, bool prune);
	void run() override;
};

class GitCommandItem_fetch_tags_f : public AbstractGitCommandItem {
public:
	GitCommandItem_fetch_tags_f(QString const &progress_message);
	void run() override;
};

class GitCommandItem_push : public AbstractGitCommandItem {
private:
	bool set_upstream_ = false;
	QString remote_;
	QString branch_;
	bool force_ = false;
private:
	friend class MainWindow;
	int exitcode_ = 0;
	QString errormsg_;
public:
	GitCommandItem_push(QString const &progress_message, bool set_upstream, QString const &remote, QString const &branch, bool force);
	void run() override;
};

class GitCommandItem_pull : public AbstractGitCommandItem {
public:
	GitCommandItem_pull(QString const &progress_message);
	void run() override;
};

class GitCommandItem_push_tags : public AbstractGitCommandItem {
public:
	GitCommandItem_push_tags(QString const &progress_message);
	void run() override;
};

class GitCommandItem_delete_tag : public AbstractGitCommandItem {
private:
	QString name_;
	bool remote_;
public:
	GitCommandItem_delete_tag(const QString &name, bool remote);
	void run() override;
};

class GitCommandItem_delete_tags : public AbstractGitCommandItem {
private:
	QStringList tagnames;
public:
	GitCommandItem_delete_tags(const QStringList &tagnames);
	void run() override;
};

class GitCommandItem_add_tag : public AbstractGitCommandItem {
private:
	QString name_;
	Git::CommitID commit_id_;
public:
	GitCommandItem_add_tag(const QString &name, const Git::CommitID &commit_id);
	void run() override;
};

class GitProcessRequest {
public:
	typedef unsigned int request_id_t;
	request_id_t request_id;
	bool override_wait_cursor = true;
	std::shared_ptr<AbstractGitCommandItem> params;
	std::function<void (GitProcessRequest const &req)> run;
	std::function<void (GitProcessRequest const &req)> done;
};
Q_DECLARE_METATYPE(GitProcessRequest)

class GitProcessThread : public QObject {
	Q_OBJECT
public:
	using request_id_t = GitProcessRequest::request_id_t;
private:
	struct Private;
	Private *m;
public:
	GitProcessThread();
	~GitProcessThread();
	void start();
	void stop();
	request_id_t request(GitProcessRequest &&req);
	void cancel(request_id_t reqid);
	bool wait();
signals:
	void done(GitProcessRequest const &req);
};

#endif // GITPROCESSTHREAD_H
