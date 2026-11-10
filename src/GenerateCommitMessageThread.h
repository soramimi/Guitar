#ifndef GENERATECOMMITMESSAGETHREAD_H
#define GENERATECOMMITMESSAGETHREAD_H
#include "GeneratedCommitMessage.h"
#include <QObject>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <optional>
#include <functional>

class GenerateCommitMessageThread : public QObject {
	Q_OBJECT
	struct Job {
		GenerativeAI::Model model;
		GenerativeAI::Credential credential;
		std::function<std::string(std::shared_ptr<std::atomic_bool>)> prepare;
		std::string hint;
		std::shared_ptr<std::atomic_bool> cancelled;
	};
	std::mutex mutex_;
	std::condition_variable cv_;
	std::thread thread_;
	bool interrupted_ = false;
	std::optional<Job> pending_;
	std::shared_ptr<std::atomic_bool> active_;
public:
	GenerateCommitMessageThread() = default;
	~GenerateCommitMessageThread();
	void start();
	void stop();
	void request(GenerativeAI::Model model, GenerativeAI::Credential credential,
	             std::function<std::string(std::shared_ptr<std::atomic_bool>)> prepare, std::string hint);
signals:
	void ready(GeneratedCommitMessage const &message);
};
#endif
