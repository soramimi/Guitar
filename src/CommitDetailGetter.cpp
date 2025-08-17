#include "CommitDetailGetter.h"
#include "ApplicationGlobal.h"

CommitDetailGetter::~CommitDetailGetter()
{
	stop();
}

/**
 * @brief CommitDetailGetter::start
 * @param git
 *
 * コミットの詳細情報を取得するためのスレッドを開始する
 */
void CommitDetailGetter::start(GitRunner git)
{
	stop();
	interrupted_ = false;
	
	git_ = git;
	
	threads_.clear();
	threads_.resize(num_threads);
	for (size_t i = 0; i < threads_.size(); i++) {
		threads_[i] = std::thread([this](){
			while (1) {
				Request item;
				{
					std::unique_lock lock(mutex_);
					if (interrupted_) return;

					auto NextQuery = [&](){
						for (size_t i = requests_.size(); i > 0; i--) {
							Request *r = &requests_[i - 1];
							if (!r->done && !r->busy) {
								r->busy = true;
								item = *r;
								break;
							}
						}
					};

					NextQuery();
					if (!item.id) {
						condition_.wait(lock);
						if (interrupted_) return;
						NextQuery();
					}
				}
				if (item.id) {
					auto c = git_.log_signature(item.id);
					if (c) {
						GitCommitItem const &commit = *c;
						item.data.sign_verify = commit.sign.verify;
					}
					item.done = true;
					item.busy = false;
					bool em = false;
					{
						std::lock_guard lock(mutex_);
						if (interrupted_) return;
						for (size_t i = 0; i < requests_.size(); i++) {
							if (item.id == requests_[i].id) {
								requests_.erase(requests_.begin() + i);
								requests_.push_back(item);
								cache_[item.id] = item.data;
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
		std::lock_guard lock(mutex_);
		interrupted_ = true;
		condition_.notify_all();
		requests_.clear();
		cache_.clear();
	}
	for (size_t i = 0; i < threads_.size(); i++) {
		if (threads_[i].joinable()) {
			threads_[i].join();
		}
	}
	threads_.clear();
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
		std::lock_guard l(mutex_);
		if (interrupted_) return {};
		return _query(id, request_if_not_found, false);
	}
	if (id) {
		auto it = cache_.find(id);
		if (it != cache_.end()) {
			return it->second;
		}

		if (request_if_not_found) {
			for (size_t i = 0; i < requests_.size(); i++) {
				auto item = requests_[i];
				if (item.id == id) {
					requests_.erase(requests_.begin() + i);
					requests_.push_back(item);
					if (item.done) {
						cache_[item.id] = item.data;
						return item.data;
					}
					return {};
				}
			}

			Request item;
			item.id = id;
			requests_.push_back(item);

			const size_t MAX = std::min(1000, global->appsettings.maximum_number_of_commit_item_acquisitions);

			size_t n = requests_.size();
			if (n > MAX) {
				n -= MAX;

				for (size_t i = 0; i < n; i++) {
					auto it = cache_.find(requests_[i].id);
					if (it != cache_.end()) {
						cache_.erase(it);
					}
				}

				requests_.erase(requests_.begin(), requests_.begin() + n);
			}

			condition_.notify_all();
		}
	}
	return {};
}

CommitDetailGetter::Data CommitDetailGetter::query(GitHash const &id, bool request_if_not_found)
{
	return _query(id, request_if_not_found, true);
}

