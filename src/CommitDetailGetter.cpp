#include "CommitDetailGetter.h"
#include "ApplicationGlobal.h"
#include "GitRunner.h"

struct CommitDetailGetter::Private {
	std::mutex mutex;
	std::condition_variable condition;
	std::vector<std::thread> threads;
	bool interrupted = false;
	GitRunner git;
	std::vector<CommitDetailGetter::Request> requests;
	std::map<GitHash, CommitDetailGetter::Data> cache;
};


CommitDetailGetter::CommitDetailGetter()
	: m(new Private)
{
}

CommitDetailGetter::~CommitDetailGetter()
{
	stop();
	delete m;
}

/**
 * @brief CommitDetailGetter::start
 * @param git
 *
 * コミットの詳細情報を取得するためのスレッドを開始する
 */
void CommitDetailGetter::start(const GitRunner &git)
{
	stop();
	m->interrupted = false;
	
	m->git = git;
	
	m->threads.clear();
	m->threads.resize(num_threads);
	for (size_t i = 0; i < m->threads.size(); i++) {
		m->threads[i] = std::thread([this](){
			while (1) {
				Request item;
				{
					std::unique_lock lock(m->mutex);
					if (m->interrupted) return;

					auto NextQuery = [&](){
						for (size_t i = m->requests.size(); i > 0; i--) {
							Request *r = &m->requests[i - 1];
							if (!r->done && !r->busy) {
								r->busy = true;
								item = *r;
								break;
							}
						}
					};

					NextQuery();
					if (!item.id) {
						m->condition.wait(lock);
						if (m->interrupted) return;
						NextQuery();
					}
				}
				if (item.id) {
					auto c = m->git.log_signature(item.id);
					if (c) {
						GitCommitItem const &commit = *c;
						item.data.sign_verify = commit.sign.verify;
					}
					item.done = true;
					item.busy = false;
					bool em = false;
					{
						std::lock_guard lock(m->mutex);
						if (m->interrupted) return;
						for (size_t i = 0; i < m->requests.size(); i++) {
							if (item.id == m->requests[i].id) {
								m->requests.erase(m->requests.begin() + i);
								m->requests.push_back(item);
								m->cache[item.id] = item.data;
								em = true;
								break;
							}
						}
					}
					if (em) {
						emit ready();
					}
				}
			}
		});
	}
}

/**
 * @brief CommitDetailGetter::stop
 *
 * スレッドを停止する
 */
void CommitDetailGetter::stop()
{
	{
		std::lock_guard lock(m->mutex);
		m->interrupted = true;
		m->condition.notify_all();
		m->requests.clear();
		m->cache.clear();
	}
	for (size_t i = 0; i < m->threads.size(); i++) {
		if (m->threads[i].joinable()) {
			m->threads[i].join();
		}
	}
	m->threads.clear();
}

/**
 * @brief CommitDetailGetter::query
 * @param id
 * @param request_if_not_found
 * @param lock
 * @return
 *
 * コミットの詳細情報を取得する
 * 情報が存在しない場合はリクエスト予約を行う
 */
CommitDetailGetter::Data CommitDetailGetter::_query(GitHash const &id, bool request_if_not_found, bool lock)
{
	if (lock) {
		std::lock_guard l(m->mutex);
		if (m->interrupted) return {};
		return _query(id, request_if_not_found, false);
	}
	if (id) {
		auto it = m->cache.find(id);
		if (it != m->cache.end()) {
			return it->second;
		}

		if (request_if_not_found) {
			for (size_t i = 0; i < m->requests.size(); i++) {
				auto item = m->requests[i];
				if (item.id == id) {
					m->requests.erase(m->requests.begin() + i);
					m->requests.push_back(item);
					if (item.done) {
						m->cache[item.id] = item.data;
						return item.data;
					}
					return {};
				}
			}

			Request item;
			item.id = id;
			m->requests.push_back(item);

			const size_t MAX = std::min(1000, global->appsettings.maximum_number_of_commit_item_acquisitions);

			size_t n = m->requests.size();
			if (n > MAX) {
				n -= MAX;

				for (size_t i = 0; i < n; i++) {
					auto it = m->cache.find(m->requests[i].id);
					if (it != m->cache.end()) {
						m->cache.erase(it);
					}
				}

				m->requests.erase(m->requests.begin(), m->requests.begin() + n);
			}

			m->condition.notify_all();
		}
	}
	return {};
}

CommitDetailGetter::Data CommitDetailGetter::query(GitHash const &id, bool request_if_not_found)
{
	return _query(id, request_if_not_found, true);
}

