#ifndef GITPROCESSTHREAD_H
#define GITPROCESSTHREAD_H

#include "ApplicationGlobal.h"
#include "Git.h"
#include "MyProcess.h"
#include <QMetaType>
#include <QObject>
#include <QString>
#include <functional>
#include <memory>

class AbstractGitCommandItem {
public:
	GitPtr g;
	PtyProcess *pty = nullptr;
	QString progress_message;
	bool update_commit_log = false;
	enum AfterOperation {
		None,
		Fetch,
		Reopen,
		UpdateFiles,
	};
	AfterOperation after_operation = Fetch;
	AbstractGitCommandItem(QString const &progress_message)
		: progress_message(progress_message)
	{
	}
	virtual ~AbstractGitCommandItem() = default;
	virtual bool run() = 0;
};

class GitCommandItem_clone : public AbstractGitCommandItem {
private:
	Git::CloneData clonedata_;
public:
	GitCommandItem_clone(QString const &progress_message, const Git::CloneData &clonedata);
	bool run() override;
};

class GitCommandItem_fetch : public AbstractGitCommandItem {
private:
	bool prune;
public:
	GitCommandItem_fetch(QString const &progress_message, bool prune);
	bool run() override;
	static std::shared_ptr<GitCommandItem_fetch> make(QString const &progress_message, bool prune)
	{
		return std::make_shared<GitCommandItem_fetch>(progress_message, prune);
	}
};

class GitCommandItem_fetch_tags_f : public AbstractGitCommandItem {
public:
	GitCommandItem_fetch_tags_f(QString const &progress_message);
	bool run() override;
	static std::shared_ptr<GitCommandItem_fetch_tags_f> make(QString const &progress_message)
	{
		return std::make_shared<GitCommandItem_fetch_tags_f>(progress_message);
	}
};

class GitCommandItem_stage : public AbstractGitCommandItem {
private:
	QStringList paths;
public:
	GitCommandItem_stage(QString const &progress_message, QStringList const &paths);
	bool run() override;
	static std::shared_ptr<GitCommandItem_stage> make(QString const &progress_message, QStringList const &paths)
	{
		return std::make_shared<GitCommandItem_stage>(progress_message, paths);
	}
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
	bool run() override;
	static std::shared_ptr<GitCommandItem_push> make(QString const &progress_message, bool set_upstream, QString const &remote, QString const &branch, bool force)
	{
		return std::make_shared<GitCommandItem_push>(progress_message, set_upstream, remote, branch, force);
	}
};

class GitCommandItem_pull : public AbstractGitCommandItem {
public:
	GitCommandItem_pull(QString const &progress_message);
	bool run() override;
	static std::shared_ptr<GitCommandItem_pull> make(QString const &label)
	{
		return std::make_shared<GitCommandItem_pull>(label);
	}
};

class GitCommandItem_push_tags : public AbstractGitCommandItem {
public:
	GitCommandItem_push_tags(QString const &progress_message);
	bool run() override;
	static std::shared_ptr<GitCommandItem_push_tags> make(QString const &label)
	{
		return std::make_shared<GitCommandItem_push_tags>(label);
	}
};

class GitCommandItem_delete_tag : public AbstractGitCommandItem {
private:
	QString name_;
	bool remote_;
public:
	GitCommandItem_delete_tag(QString const &progress_message, const QString &name, bool remote);
	bool run() override;
	static std::shared_ptr<GitCommandItem_delete_tag> make(QString const &progress_message, const QString &name, bool remote)
	{
		return std::make_shared<GitCommandItem_delete_tag>(progress_message, name, remote);
	}
};

class GitCommandItem_delete_tags : public AbstractGitCommandItem {
private:
	QStringList tagnames;
public:
	GitCommandItem_delete_tags(QString const &progress_message, const QStringList &tagnames);
	bool run() override;
	static std::shared_ptr<GitCommandItem_delete_tags> make(QString const &progress_message, const QStringList &tagnames)
	{
		return std::make_shared<GitCommandItem_delete_tags>(progress_message, tagnames);
	}
};

class GitCommandItem_add_tag : public AbstractGitCommandItem {
private:
	QString name_;
	Git::CommitID commit_id_;
public:
	GitCommandItem_add_tag(QString const &progress_message, const QString &name, const Git::CommitID &commit_id);
	bool run() override;
	static std::shared_ptr<GitCommandItem_add_tag> make(QString const &progress_message, const QString &name, const Git::CommitID &commit_id)
	{
		return std::make_shared<GitCommandItem_add_tag>(progress_message, name, commit_id);
	}
};

class GitCommandItem_submodule_add : public AbstractGitCommandItem {
private:
	Git::CloneData data_;
	bool force_ = false;
public:
	GitCommandItem_submodule_add(QString const &progress_message, Git::CloneData data, bool force);
	bool run() override;
	static std::shared_ptr<GitCommandItem_submodule_add> make(QString const &progress_message, Git::CloneData data, bool force)
	{
		return std::make_shared<GitCommandItem_submodule_add>(progress_message, data, force);
	}
};


class GitProcessRequest {
public:
	typedef unsigned int request_id_t;
	request_id_t request_id;
	bool override_wait_cursor = true;
	std::shared_ptr<AbstractGitCommandItem> params;
	std::function<void (GitProcessRequest const &req)> run;
	QVariant userdata;
	std::function<void (ProcessStatus const &status, QVariant const &userdata)> callback;
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
	// void done(GitProcessRequest const &req);
};

#endif // GITPROCESSTHREAD_H
