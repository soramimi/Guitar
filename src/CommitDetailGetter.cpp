#include "CommitDetailGetter.h"

CommitDetailGetter::~CommitDetailGetter()
{
	stop();
}

void CommitDetailGetter::start(GitPtr git)
{
	interrupted_ = false;
	git_ = git;
	thread_ = std::make_shared<std::thread>([&](){
		while (1) {
			Item item;
			{
				std::unique_lock lock(mutex_);
				if (interrupted_) return;
				condition_.wait(lock);
				if (interrupted_) return;
				for (size_t i = 0; i < requests_.size(); i++) {
					if (!requests_[i].done && !requests_[i].busy) {
						requests_[i].busy = true;
						item = requests_[i];
						break;
					}
				}
			}
			if (item.id) {
				auto c = git_->log_signature(item.id);
				if (c) {
					bool ok = false;
					Git::CommitItem const &commit = *c;
					item.value = commit.sign.verify;
					item.done = true;
					item.busy = false;
					{
						std::lock_guard lock(mutex_);
						for (size_t i = 0; i < requests_.size(); i++) {
							if (item.id == requests_[i].id) {
								requests_.erase(requests_.begin() + i);
								requests_.push_back(item);
								requests_[i] = item;
								ok = true;
								break;
							}
						}
					}
					if (ok) {
						emit ready();
					}
				}
			}
		}
	});
}

void CommitDetailGetter::stop()
{
	{
		std::lock_guard lock(mutex_);
		interrupted_ = true;
		condition_.notify_all();
	}
	if (thread_) {
		if (thread_->joinable()) {
			thread_->join();
		}
		thread_.reset();
	}
	interrupted_ = false;
}

CommitDetailGetter::Item CommitDetailGetter::request(Git::CommitID id)
{
	if (id.isValid()) {
		std::lock_guard lock(mutex_);
		for (size_t i = 0; i < requests_.size(); i++) {
			if (id == requests_[i].id) {
				if (requests_[i].done) {
					Item item = requests_[i];
					requests_.erase(requests_.begin() + i);
					requests_.push_back(item);
					return item;
				}
			}
		}

		Item item;
		item.id = id;
		requests_.push_back(item);

		size_t n = requests_.size();
		if (n > 100) {
			n -= 100;
			requests_.erase(requests_.begin(), requests_.begin() + n);
		}

		condition_.notify_all();
	}
	return {};
}

void CommitDetailGetter::apply(Git::CommitItemList *logs)
{
	std::lock_guard lock(mutex_);
	for (size_t i = 0; i < requests_.size(); i++) {
		if (requests_[i].done) {
			Git::CommitID const &id = requests_[i].id;
			for (size_t j = 0; j < logs->size(); j++) {
				if (id == (*logs)[j].commit_id) {
					(*logs)[j].sign.verify = requests_[i].value;
					break;
				}
			}
		}
	}
}
