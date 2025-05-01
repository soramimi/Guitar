#include "GenerateCommitMessageThread.h"
#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include "GeneratedCommitMessage.h"

GenerateCommitMessageThread::GenerateCommitMessageThread()
{
}

GenerateCommitMessageThread::~GenerateCommitMessageThread()
{
	stop();
}

void GenerateCommitMessageThread::start()
{
	stop();
	interrupted_ = false;
	thread_ = std::thread([this](){
		while (1) {
			bool requested = false;
			{
				std::unique_lock lock(mutex_);
				if (interrupted_)	break;
				cv_.wait(lock);
				std::swap(requested, requested_);
			}
			if (requested) {
				CommitMessageGenerator gen(kind_);
				auto result = GeneratedCommitMessage(new CommitMessageGenerator::Result(gen.generate(diff_)));
				emit ready(result);
			}
		}
	});	
	
}

void GenerateCommitMessageThread::stop()
{
	interrupted_ = true;
	cv_.notify_all();
	if (thread_.joinable()) {
		thread_.join();
	}
}

void GenerateCommitMessageThread::request(CommitMessageGenerator::Kind kind, QString const &diff, QString const &hint)
{
	std::lock_guard lock(mutex_);
	kind_ = kind;
	diff_ = diff;
	hint_ = hint;
	requested_ = true;
	cv_.notify_all();
	
}
