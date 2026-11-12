#include "GitProcessThread.h"
#include <QApplication>
#include <QEventLoop>
#include <mutex>
#include <condition_variable>
#include <algorithm>

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
	{
		std::lock_guard lock(m->mutex_);
		m->interrrupt_ = false;
	}
	m->thread_ = std::thread([this]() {
		while (1) {
			GitCommandRunner req;
			{
				// Wait on the queue state rather than on a bare notification, so
				// requests queued while another one is running are not lost and a
				// spurious wakeup never reads from an empty queue.
				std::unique_lock lock(m->mutex_);
				m->cv_request.wait(lock, [this]{ return m->interrrupt_ || !m->requests_.empty(); });
				if (m->interrrupt_) break;
				req = *m->requests_.front();
			}
			req.d.run(req);
			{
				// The request may have been canceled while it was running, so
				// remove it by ID instead of assuming it is still at the front.
				std::lock_guard lock(m->mutex_);
				auto it = std::find_if(m->requests_.begin(), m->requests_.end(), [&](auto const &r){
					return r->d.request_id == req.d.request_id;
				});
				if (it != m->requests_.end()) {
					m->requests_.erase(it);
				}
			}
			m->cv_done.notify_all();
		}
	});
}

void GitProcessThread::stop()
{
	{
		std::lock_guard lock(m->mutex_);
		m->interrrupt_ = true;
	}
	m->cv_request.notify_all();
	m->cv_done.notify_all();
	if (m->thread_.joinable()) {
		m->thread_.join();
	}
}

GitProcessThread::request_id_t GitProcessThread::request(GitCommandRunner &&req)
{
	std::unique_lock lock(m->mutex_);
	m->requests_.push_back(std::make_shared<GitCommandRunner>(std::move(req)));
	GitCommandRunner::request_id_t reqid = m->next_request_id++;
	m->requests_.back()->d.request_id = reqid;
	m->cv_request.notify_all();
	return reqid;
}

void GitProcessThread::cancel(request_id_t reqid)
{
	std::unique_lock lock(m->mutex_);
	size_t i = m->requests_.size();
	while (i > 0) {
		i--;
		if (m->requests_[i]->d.request_id == reqid) {
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

