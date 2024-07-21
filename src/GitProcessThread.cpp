#include "GitProcessThread.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "MainWindow.h"

struct GitProcessThread::Private {
	std::mutex mutex_;
	std::condition_variable cv_request;
	std::condition_variable cv_done;
	std::thread thread_;
	bool interrrupt_ = false;
	std::vector<std::shared_ptr<GitProcessRequest>> requests_;
	GitProcessRequest::request_id_t next_request_id = 0;
};

GitProcessThread::GitProcessThread()
	: m(new Private)
{
}

GitProcessThread::~GitProcessThread()
{
	delete m;
}

void GitProcessThread::start()
{
	stop();
	m->interrrupt_ = false;
	m->thread_ = std::thread([&]() {
		while (1) {
			GitProcessRequest req;
			{
				std::unique_lock lock(m->mutex_);
				if (m->interrrupt_) break;
				m->cv_request.wait(lock);
				if (m->interrrupt_) break;
				req = *m->requests_.front();
			}
			req.run(req);
			bool ok = false;
			{
				std::unique_lock lock(m->mutex_);
				if (m->requests_.front()->request_id == req.request_id) {
					m->requests_.erase(m->requests_.begin());
					ok = true;
				}
				if (m->interrrupt_) break;
			}
			if (ok) {
				m->cv_done.notify_all();
			}
		}
	});
}

void GitProcessThread::stop()
{
	m->interrrupt_ = true;
	m->cv_request.notify_all();
	if (m->thread_.joinable()) {
		m->thread_.join();
	}
}

GitProcessThread::request_id_t GitProcessThread::request(GitProcessRequest &&req)
{
	std::unique_lock lock(m->mutex_);
	m->requests_.push_back(std::make_shared<GitProcessRequest>(std::move(req)));
	GitProcessRequest::request_id_t reqid = m->next_request_id++;
	m->requests_.back()->request_id = reqid;
	m->cv_request.notify_all();
	return reqid;
}

void GitProcessThread::cancel(request_id_t reqid)
{
	std::unique_lock lock(m->mutex_);
	size_t i = m->requests_.size();
	while (i > 0) {
		i--;
		if (m->requests_[i]->request_id == reqid) {
			m->requests_.erase(m->requests_.begin() + i);
		}
	}
}

bool GitProcessThread::wait()
{
	while (1) {
		{
			std::unique_lock lock(m->mutex_);
			if (m->interrrupt_) return false;
			if (m->requests_.empty()) return true;
			if (m->cv_done.wait_for(lock, std::chrono::milliseconds(1)) == std::cv_status::timeout) continue;
		}
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	}
}


GitCommandItem_clone::GitCommandItem_clone(const QString &progress_message, Git::CloneData const &clonedata)
	: AbstractGitCommandItem(progress_message)
	, clonedata_(clonedata)
{
	
}

bool GitCommandItem_clone::run()
{
	return g->clone(clonedata_, pty);
}

GitCommandItem_fetch::GitCommandItem_fetch(const QString &progress_message, bool prune)
	: AbstractGitCommandItem(progress_message)
	, prune(prune)
{
	after_operation = Reopen;
	update_commit_log = true;
}

bool GitCommandItem_fetch::run()
{
	return g->fetch(pty, prune);
}

//

GitCommandItem_stage::GitCommandItem_stage(const QString &progress_message, QStringList const &paths)
	: AbstractGitCommandItem(progress_message)
	, paths(paths)
{
	after_operation = UpdateFiles;
}

bool GitCommandItem_stage::run()
{
	return g->stage(paths, pty);
}

//

GitCommandItem_fetch_tags_f::GitCommandItem_fetch_tags_f(const QString &progress_message)
	: AbstractGitCommandItem(progress_message)
{
	update_commit_log = true;
}

bool GitCommandItem_fetch_tags_f::run()
{
	return g->fetch_tags_f(pty);
}

//

GitCommandItem_push::GitCommandItem_push(const QString &progress_message, bool set_upstream, QString const &remote, QString const &branch, bool force)
	: AbstractGitCommandItem(progress_message)
	, set_upstream_(set_upstream)
	, remote_(remote)
	, branch_(branch)
	, force_(force)
{
}

bool GitCommandItem_push::run()
{
	return g->push_u(set_upstream_, remote_, branch_, force_, pty);
}

//

GitCommandItem_pull::GitCommandItem_pull(const QString &progress_message)
	: AbstractGitCommandItem(progress_message)
{
}

bool GitCommandItem_pull::run()
{
	return g->pull(pty);
}

GitCommandItem_push_tags::GitCommandItem_push_tags(const QString &progress_message)
	: AbstractGitCommandItem(progress_message)
{
}

bool GitCommandItem_push_tags::run()
{
	return g->push_tags(pty);
}

GitCommandItem_delete_tag::GitCommandItem_delete_tag(const QString &name, bool remote)
	: AbstractGitCommandItem(QString())
	, name_(name)
	, remote_(remote)
{
}

bool GitCommandItem_delete_tag::run()
{
	return g->delete_tag(name_, remote_);
}

GitCommandItem_delete_tags::GitCommandItem_delete_tags(const QStringList &tagnames)
	: AbstractGitCommandItem(QString())
	, tagnames(tagnames)
{
}

bool GitCommandItem_delete_tags::run()
{
	bool ok = false;
	for (QString const &name : tagnames) {
		if (g->delete_tag(name, true)) {
			ok = true;
		}
	}
	return ok;
}

GitCommandItem_add_tag::GitCommandItem_add_tag(const QString &name, Git::CommitID const &commit_id)
	: AbstractGitCommandItem(QString())
	, name_(name)
	, commit_id_(commit_id)
{
}

bool GitCommandItem_add_tag::run()
{
	return g->tag(name_, commit_id_);
}


GitCommandItem_submodule_add::GitCommandItem_submodule_add(const QString &progress_message, Git::CloneData data, bool force)
	: AbstractGitCommandItem(progress_message)
	, data_(data)
	, force_(force)
{
}

bool GitCommandItem_submodule_add::run()
{
	return g->submodule_add(data_, force_, pty);
}
