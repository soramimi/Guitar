#include "GitProcessThread.h"
#include "MainWindow.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

struct GitProcessThread::Private {
	std::mutex mutex_;
	std::condition_variable cv_request;
	std::condition_variable cv_done;
	std::thread thread_;
	bool interrrupt_ = false;
	std::vector<std::shared_ptr<GitCommandRunner>> requests_;
	GitCommandRunner::request_id_t next_request_id = 0;
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
			GitCommandRunner req;
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

GitProcessThread::request_id_t GitProcessThread::request(GitCommandRunner &&req)
{
	std::unique_lock lock(m->mutex_);
	m->requests_.push_back(std::make_shared<GitCommandRunner>(std::move(req)));
	GitCommandRunner::request_id_t reqid = m->next_request_id++;
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

//

std::string GitCommandRunner::pty_message() const
{
	return pty_->getMessage();
}

void GitCommandRunner::operator ()(GitCommandItem_clone const &item)
{
	result = git()->clone(item.clonedata_, pty());
}

void GitCommandRunner::operator ()(GitCommandItem_fetch const &item)
{
	result = git()->fetch(pty(), item.prune);
}

void GitCommandRunner::operator ()(GitCommandItem_fetch_tags_f const &item)
{
	result = git()->fetch_tags_f(pty());
}

void GitCommandRunner::operator ()(GitCommandItem_stage const &item)
{
	result = git()->stage(item.paths, pty());
}

void GitCommandRunner::operator ()(GitCommandItem_push const &item)
{
	result = git()->push_u(item.set_upstream_, item.remote_, item.branch_, item.force_, pty());
}

void GitCommandRunner::operator ()(GitCommandItem_pull const &item)
{
	result = git()->pull(pty());
}

void GitCommandRunner::operator ()(GitCommandItem_push_tags const &item)
{
	result = git()->push_tags(pty());
}

void GitCommandRunner::operator ()(GitCommandItem_delete_tag const &item)
{
	result = git()->delete_tag(item.name_, item.remote_);
}

void GitCommandRunner::operator ()(GitCommandItem_delete_tags const &item)
{
	result = false;
	for (QString const &name : item.tagnames) {
		if (git()->delete_tag(name, true)) {
			result = true;
		}
	}
}

void GitCommandRunner::operator ()(GitCommandItem_add_tag const &item)
{
	result = git()->tag(item.name_, item.commit_id_);
}

void GitCommandRunner::operator ()(GitCommandItem_submodule_add const &item)
{
	result = git()->submodule_add(item.data_, item.force_, pty());
}
