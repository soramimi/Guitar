#include "GenerateCommitMessageThread.h"
#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include "GeneratedCommitMessage.h"

GenerateCommitMessageThread::GenerateCommitMessageThread()
{
	ai_model_ = global->appsettings.ai_model;
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
				CommitMessageGenerator::Request request(diff_, status_s_u_, hint_);
				CommitMessageGenerator gen(ai_model_, request);
				gen.set_ai_model(ai_model_);
				auto r = gen.request();
				auto r2 = CommitMessageGenerator::parse_response(ai_model_, r);
				auto result = GeneratedCommitMessage(new CommitMessageGenerator::CommitMessageGenerator::Result(r2));
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

void GenerateCommitMessageThread::request(GenerativeAI::Model ai_model, const std::string &diff, std::string const &status_s_u, std::string const &hint)
{
	std::lock_guard lock(mutex_);
	ai_model_ = ai_model;
	diff_ = diff;
	status_s_u_ = status_s_u;
	hint_ = hint;
	requested_ = true;
	cv_.notify_all();
	
}
