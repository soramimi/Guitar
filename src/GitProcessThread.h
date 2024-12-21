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

class GitCommandItem_clone {
public:
	Git::CloneData clonedata_;
	GitCommandItem_clone(const Git::CloneData &clonedata)
		: clonedata_(clonedata)
	{

	}
};

class GitCommandItem_fetch {
public:
	bool prune;
	GitCommandItem_fetch(bool prune)
		: prune(prune)
	{
	}
	static std::shared_ptr<GitCommandItem_fetch> make(bool prune)
	{
		return std::make_shared<GitCommandItem_fetch>(prune);
	}
};

class GitCommandItem_fetch_tags_f {
public:
	GitCommandItem_fetch_tags_f()
	{
	}
	static std::shared_ptr<GitCommandItem_fetch_tags_f> make()
	{
		return std::make_shared<GitCommandItem_fetch_tags_f>();
	}
};

class GitCommandItem_stage {
public:
	QStringList paths;
	GitCommandItem_stage(QStringList const &paths)
		: paths(paths)
	{
	}
	static std::shared_ptr<GitCommandItem_stage> make(QStringList const &paths)
	{
		return std::make_shared<GitCommandItem_stage>(paths);
	}
};

class GitCommandItem_push {
public:
	bool set_upstream_ = false;
	QString remote_;
	QString branch_;
	bool force_ = false;
	int exitcode_ = 0;
	QString errormsg_;
	GitCommandItem_push(bool set_upstream, QString const &remote, QString const &branch, bool force)
		: set_upstream_(set_upstream)
		, remote_(remote)
		, branch_(branch)
		, force_(force)
	{
	}
	static std::shared_ptr<GitCommandItem_push> make(bool set_upstream, QString const &remote, QString const &branch, bool force)
	{
		return std::make_shared<GitCommandItem_push>(set_upstream, remote, branch, force);
	}
};

class GitCommandItem_pull {
public:
	GitCommandItem_pull()
	{
	}
	static std::shared_ptr<GitCommandItem_pull> make()
	{
		return std::make_shared<GitCommandItem_pull>();
	}
};

class GitCommandItem_push_tags {
public:
	GitCommandItem_push_tags()
	{
	}
	static std::shared_ptr<GitCommandItem_push_tags> make()
	{
		return std::make_shared<GitCommandItem_push_tags>();
	}
};

class GitCommandItem_delete_tag {
public:
	QString name_;
	bool remote_;
	GitCommandItem_delete_tag(const QString &name, bool remote)
		: name_(name)
		, remote_(remote)
	{
	}
	static std::shared_ptr<GitCommandItem_delete_tag> make(const QString &name, bool remote)
	{
		return std::make_shared<GitCommandItem_delete_tag>(name, remote);
	}
};

class GitCommandItem_delete_tags {
public:
	QStringList tagnames;
	GitCommandItem_delete_tags(const QStringList &tagnames)
		: tagnames(tagnames)
	{
	}
	static std::shared_ptr<GitCommandItem_delete_tags> make(const QStringList &tagnames)
	{
		return std::make_shared<GitCommandItem_delete_tags>(tagnames);
	}
};

class GitCommandItem_add_tag {
public:
	QString name_;
	Git::CommitID commit_id_;
	GitCommandItem_add_tag(const QString &name, const Git::CommitID &commit_id)
		: name_(name)
		, commit_id_(commit_id)
	{
	}
	static std::shared_ptr<GitCommandItem_add_tag> make(const QString &name, const Git::CommitID &commit_id)
	{
		return std::make_shared<GitCommandItem_add_tag>(name, commit_id);
	}
};

class GitCommandItem_submodule_add {
public:
	Git::CloneData data_;
	bool force_ = false;
	GitCommandItem_submodule_add(Git::CloneData data, bool force)
		: data_(data)
		, force_(force)
	{
	}
	static std::shared_ptr<GitCommandItem_submodule_add> make(Git::CloneData data, bool force)
	{
		return std::make_shared<GitCommandItem_submodule_add>(data, force);
	}
};

class GitCommandRunner {
public:
	void operator () (GitCommandItem_clone const &item);
	void operator () (GitCommandItem_fetch const &item);
	void operator () (GitCommandItem_fetch_tags_f const &item);
	void operator () (GitCommandItem_stage const &item);
	void operator () (GitCommandItem_push const &item);
	void operator () (GitCommandItem_pull const &item);
	void operator () (GitCommandItem_push_tags const &item);
	void operator () (GitCommandItem_delete_tag const &item);
	void operator () (GitCommandItem_delete_tags const &item);
	void operator () (GitCommandItem_add_tag const &item);
	void operator () (GitCommandItem_submodule_add const &item);

	typedef std::variant<
		GitCommandItem_clone,
		GitCommandItem_fetch,
		GitCommandItem_fetch_tags_f,
		GitCommandItem_stage,
		GitCommandItem_push,
		GitCommandItem_pull,
		GitCommandItem_push_tags,
		GitCommandItem_delete_tag,
		GitCommandItem_delete_tags,
		GitCommandItem_add_tag,
		GitCommandItem_submodule_add
		> variant_t;

	GitPtr g_;
	PtyProcess *pty_ = nullptr;
	typedef unsigned int request_id_t;
	request_id_t request_id;
	bool override_wait_cursor = true;
	std::function<void (GitCommandRunner &req)> run;
	QVariant userdata;
	bool update_commit_log = false;
	bool result = false;

	GitPtr git()
	{
		return g_;
	}
	PtyProcess *pty()
	{
		return pty_;
	}
	std::string pty_message() const;
	PtyProcess const *pty() const
	{
		return pty_;
	}
	std::function<void (ProcessStatus const &status, QVariant const &)> callback;
};
Q_DECLARE_METATYPE(GitCommandRunner)

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
