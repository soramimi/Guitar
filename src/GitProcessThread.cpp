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
				emit done(req);
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

GitCommandItem_fetch::GitCommandItem_fetch(const QString &progress_message, bool prune)
	: AbstractGitCommandItem(progress_message)
	, prune(prune)
{
	update_commit_log = true;
}

//

void GitCommandItem_fetch::run()
{
	g->fetch(pty, prune);
	global->mainwindow->runFetch_(g);
}

//

GitCommandItem_fetch_tags_f::GitCommandItem_fetch_tags_f(const QString &progress_message)
	: AbstractGitCommandItem(progress_message)
{
	update_commit_log = true;
}

void GitCommandItem_fetch_tags_f::run()
{
	g->fetch_tags_f(pty);
}

//

GitCommandItem_pull::GitCommandItem_pull(const QString &progress_message)
	: AbstractGitCommandItem(progress_message)
{
}

void GitCommandItem_pull::run()
{
	g->pull(pty);
}

GitCommandItem_push_tags::GitCommandItem_push_tags(const QString &progress_message)
	: AbstractGitCommandItem(progress_message)
{
}

void GitCommandItem_push_tags::run()
{
	g->push_tags(pty);
}

GitCommandItem_delete_tag::GitCommandItem_delete_tag(const QString &name, bool remote)
	: AbstractGitCommandItem(QString())
	, name_(name)
	, remote_(remote)
{
}

void GitCommandItem_delete_tag::run()
{
	g->delete_tag(name_, remote_);
}

GitCommandItem_delete_tags::GitCommandItem_delete_tags(const QStringList &tagnames)
	: AbstractGitCommandItem(QString())
	, tagnames(tagnames)
{
}

void GitCommandItem_delete_tags::run()
{
	for (QString const &name : tagnames) {
		g->delete_tag(name, true);
	}
}

GitCommandItem_add_tag::GitCommandItem_add_tag(const QString &name, Git::CommitID const &commit_id)
	: AbstractGitCommandItem(QString())
	, name_(name)
	, commit_id_(commit_id)
{
}

void GitCommandItem_add_tag::run()
{
	g->tag(name_, commit_id_);
}
