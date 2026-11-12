#include "GitProcessThread.h"
#include <QApplication>
#include <QEventLoop>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <optional>

struct GitProcessThread::Private {
	std::mutex mutex_;
	std::condition_variable cv_request;
	std::condition_variable cv_done;
	std::thread thread_;
	bool interrrupt_ = false;
	std::optional<request_id_t> active_request;
	bool completed = false;
	std::vector<std::shared_ptr<GitCommandRunner>> requests_;
	GitCommandRunner::request_id_t next_request_id = 0;
};

GitProcessThread::GitProcessThread()
	: m(new Private)
{
}

GitProcessThread::~GitProcessThread()
{
	stop();
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
				m->active_request = req.d.request_id;
				m->completed = false;
			}
			req.d.run(req);
			{
				// The request may have been canceled while it was running, so
				// remove it by ID instead of assuming it is still at the front.
				std::unique_lock lock(m->mutex_);
				if (req.d.wait_for_completion) {
					m->cv_done.wait(lock, [this] { return m->interrrupt_ || m->completed; });
				}
				m->active_request.reset();
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
	std::lock_guard lock(m->mutex_);
	m->requests_.clear();
	m->active_request.reset();
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
			if (m->requests_.empty() && !m->active_request) return true;
			m->cv_done.wait_for(lock, std::chrono::milliseconds(1));
		}
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	}
}


void GitProcessThread::complete(request_id_t reqid)
{
	{
		std::lock_guard lock(m->mutex_);
		if (m->active_request != reqid) return;
		m->completed = true;
	}
	m->cv_done.notify_all();
}

bool GitProcessThread::busy() const
{
	std::lock_guard lock(m->mutex_);
	return m->active_request.has_value() || !m->requests_.empty();
}
