#ifndef ABSTRACTPROCESS_H
#define ABSTRACTPROCESS_H

#include "ProcessHelper.h"
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <limits.h>

#ifdef APP_GUITAR
#include <QObject>
#include <QVariant>
#endif

class QString;

class AbstractProcess {
public:
	virtual ~AbstractProcess() { }

	virtual void start(std::string const &command, bool use_input) = 0;
	virtual bool wait(int time = INT_MAX) = 0;
	virtual void stop() = 0;
	virtual bool is_running() const = 0;
	virtual int get_exit_code() const = 0;
	virtual void write_input(char const *ptr, int len) = 0;

	virtual void close_input() = 0;

	virtual std::vector<char> const &stdout_bytes() const = 0;
	virtual std::vector<char> const &stderr_bytes() const = 0;
};

class AbstractPtyProcess {
protected:
	mutable std::mutex mutex_;
	std::condition_variable cond_;

	process::helper::dir_string_t change_dir_;

	std::shared_ptr<void> user_data_;
	std::function<void(bool, std::shared_ptr<void>)> completed_fn_;

	std::deque<char> output_queue_; // for log
	std::vector<char> output_vector_; // for result
	std::vector<char> stderr_bytes_;

	size_t max_output_queue_size_ = 0; // 0 = unlimited

	void write_output(char const *buf, size_t len);
	int pop_output(char *ptr, int len);

public:
	virtual ~AbstractPtyProcess() { }

	void set_change_dir(process::helper::dir_string_t const &dir)
	{
		change_dir_ = dir;
	}

	// output_queue_ の最大サイズを設定する (0 = 無制限)。
	// 超過分は古いデータから捨てられる。output_vector_ (結果バッファ) には影響しない。
	void set_max_output_queue_size(size_t n)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		max_output_queue_size_ = n;
	}

	// start() 前に設定すること。実行中の変更はスレッドセーフではない。
	void set_completion_callback(std::function<void(bool, std::shared_ptr<void>)> fn, std::shared_ptr<void> userdata)
	{
		completed_fn_ = fn;
		user_data_ = userdata;
	}

	void notify_completed()
	{
		if (completed_fn_) {
			completed_fn_(true, user_data_);
		}
	}

	std::string get_message() const; // deprecated
	void clear_message();

	std::vector<char> const &stdout_bytes() const
	{
		return output_vector_;
	}
	std::vector<char> const &stderr_bytes() const
	{
		return stderr_bytes_;
		// return output
	}

	virtual void start(std::string const &cmd, std::string const &env, bool use_input) = 0;
	virtual bool wait(int time = INT_MAX) = 0;
	virtual void stop() = 0;
	virtual bool is_running() const = 0;
	virtual int get_exit_code() const = 0;
	virtual void write_input(char const *ptr, int len) = 0;
	virtual int read_output(char *ptr, int len) = 0;
	virtual void close_input() = 0;
};

#endif // ABSTRACTPROCESS_H
