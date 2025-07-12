#ifndef GENERATECOMMITMESSAGETHREAD_H
#define GENERATECOMMITMESSAGETHREAD_H

#include <QObject>
#include "CommitMessageGenerator.h"
#include "GeneratedCommitMessage.h"
#include <condition_variable>
#include <mutex>
#include <thread>

class GenerateCommitMessageThread : public QObject {
	Q_OBJECT
public:
	CommitMessageGenerator gen_;
	std::mutex mutex_;
	std::thread thread_;
	std::condition_variable cv_;
	bool requested_ = false;
	bool interrupted_ = false;
	std::string diff_;
	QString hint_;
	
	GenerateCommitMessageThread();
	~GenerateCommitMessageThread();
	void start();
	void stop();
	void request(std::string const &diff, QString const &hint = {});
signals:
	void ready(GeneratedCommitMessage const &message);
	
};

#endif // GENERATECOMMITMESSAGETHREAD_H
