#ifndef GENERATECOMMITMESSAGETHREAD_H
#define GENERATECOMMITMESSAGETHREAD_H

#include <QObject>
#include "CommitMessageGenerator.h"
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
	CommitMessageGenerator::Kind kind_;
	QString diff_;
	QString hint_;
	
	GenerateCommitMessageThread();
	~GenerateCommitMessageThread();
	void start();
	void stop();
	void request(CommitMessageGenerator::Kind kind, const QString &diff, QString const &hint = {});
signals:
	void ready(GeneratedCommitMessage const &message);
	
};

#endif // GENERATECOMMITMESSAGETHREAD_H
