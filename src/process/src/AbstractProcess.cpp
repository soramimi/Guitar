#include "AbstractProcess.h"
#include <algorithm>

void AbstractPtyProcess::write_output(char const *buf, size_t len)
{
	std::lock_guard<std::mutex> lock(mutex_);
	output_queue_.insert(output_queue_.end(), buf, buf + len);
	if (max_output_queue_size_ > 0) {
		while (output_queue_.size() > max_output_queue_size_) {
			output_queue_.pop_front();
		}
	}
	output_vector_.insert(output_vector_.end(), buf, buf + len);
}

int AbstractPtyProcess::pop_output(char *ptr, int len)
{
	if (!ptr || len <= 0) {
		return 0;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	int n = std::min(len, static_cast<int>(output_queue_.size()));
	if (n > 0) {
		auto it = output_queue_.begin();
		std::copy(it, it + n, ptr);
		output_queue_.erase(it, it + n);
	}
	return n;
}

std::string AbstractPtyProcess::get_message() const // deprecated
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (output_vector_.empty()) return { };
	return std::string(&output_vector_[0], output_vector_.size());
}

void AbstractPtyProcess::clear_message()
{
	std::lock_guard<std::mutex> lock(mutex_);
	output_vector_.clear();
	output_vector_.clear();
}
