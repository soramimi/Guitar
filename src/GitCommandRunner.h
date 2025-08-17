#ifndef GITCOMMANDRUNNER_H
#define GITCOMMANDRUNNER_H

#include "GitRunner.h"
#include "MyProcess.h"

#include <QElapsedTimer>

class Git_clone {
public:
	GitCloneData clonedata_;
	Git_clone(const GitCloneData &clonedata)
		: clonedata_(clonedata)
	{
	}
};

class Git_fetch {
public:
	bool prune;
	Git_fetch(bool prune)
		: prune(prune)
	{
	}
};

class Git_stage {
public:
	QStringList paths;
	Git_stage(QStringList const &paths)
		: paths(paths)
	{
	}
};

class Git_push {
public:
	bool set_upstream_ = false;
	QString remote_;
	QString branch_;
	bool force_ = false;
	int exitcode_ = 0;
	QString errormsg_;
	Git_push(bool set_upstream, QString const &remote, QString const &branch, bool force)
		: set_upstream_(set_upstream)
		, remote_(remote)
		, branch_(branch)
		, force_(force)
	{
	}
};

class Git_pull {
public:
	Git_pull()
	{
	}
};

class Git_push_tags {
public:
	Git_push_tags()
	{
	}
};

class Git_delete_tag {
public:
	QString name_;
	bool remote_;
	Git_delete_tag(const QString &name, bool remote)
		: name_(name)
		, remote_(remote)
	{
	}
};

class Git_delete_tags {
public:
	QStringList tagnames;
	Git_delete_tags(const QStringList &tagnames)
		: tagnames(tagnames)
	{
	}
};

class Git_add_tag {
public:
	QString name_;
	GitHash commit_id_;
	Git_add_tag(const QString &name, const GitHash &commit_id)
		: name_(name)
		, commit_id_(commit_id)
	{
	}
};

class Git_submodule_add {
public:
	GitCloneData data_;
	bool force_ = false;
	Git_submodule_add(GitCloneData data, bool force)
		: data_(data)
		, force_(force)
	{
	}
};

class GitCommandRunner {
public:
	void operator () (Git_clone const &item);
	void operator () (Git_fetch const &item);
	void operator () (Git_stage const &item);
	void operator () (Git_push const &item);
	void operator () (Git_pull const &item);
	void operator () (Git_push_tags const &item);
	void operator () (Git_delete_tag const &item);
	void operator () (Git_delete_tags const &item);
	void operator () (Git_add_tag const &item);
	void operator () (Git_submodule_add const &item);

	typedef std::variant<
		Git_clone,
		Git_fetch,
		Git_stage,
		Git_push,
		Git_pull,
		Git_push_tags,
		Git_delete_tag,
		Git_delete_tags,
		Git_add_tag,
		Git_submodule_add
		> variant_t;

	typedef unsigned int request_id_t;

	struct D {
		GitRunner g;
		PtyProcess *pty = nullptr;
		request_id_t request_id = 0;
		bool override_wait_cursor = true;
		std::function<void (GitCommandRunner &req)> run;
		QVariant userdata;
		bool update_commit_log = false;
		bool result = false;
		QString process_name;
		QElapsedTimer elapsed;
	} d;

	GitRunner git()
	{
		return d.g;
	}
	PtyProcess *pty()
	{
		return d.pty;
	}
	std::string pty_message() const;
	PtyProcess const *pty() const
	{
		return d.pty;
	}
	std::function<void (ProcessStatus *status, QVariant const &)> callback;
};
Q_DECLARE_METATYPE(GitCommandRunner)

#endif // GITCOMMANDRUNNER_H
