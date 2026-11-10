#include "GenerateCommitMessageThread.h"

GenerateCommitMessageThread::~GenerateCommitMessageThread() { stop(); }
void GenerateCommitMessageThread::start()
{
	stop();
	{ std::lock_guard lock(mutex_); interrupted_ = false; }
	thread_ = std::thread([this] {
		while (true) {
			Job job;
			{
				std::unique_lock lock(mutex_);
				cv_.wait(lock, [this] { return interrupted_ || pending_.has_value(); });
				if (interrupted_) return;
				job = std::move(*pending_); pending_.reset(); active_ = job.cancelled;
			}
			CommitMessageGenerator::Result result;
			try {
				auto diff = job.prepare(job.cancelled);
				if (!job.cancelled->load()) {
					CommitMessageGenerator generator(job.model, {diff, "", job.hint});
					generator.set_ai_model(job.model, job.credential);
					generator.set_cancellation_token(job.cancelled);
					result = CommitMessageGenerator::parse_response(job.model, generator.request());
				}
			} catch (std::exception const &e) {
				result.error = true; result.error_message = e.what();
			} catch (...) {
				result.error = true; result.error_message = "Commit message generation failed.";
			}
			std::lock_guard lock(mutex_);
			if (!interrupted_ && !job.cancelled->load()) emit ready(GeneratedCommitMessage(new CommitMessageGenerator::Result(std::move(result))));
			active_.reset();
		}
	});
}
void GenerateCommitMessageThread::stop()
{
	{
		std::lock_guard lock(mutex_); interrupted_ = true;
		if (active_) active_->store(true);
		if (pending_) pending_->cancelled->store(true);
		pending_.reset();
	}
	cv_.notify_all();
	if (thread_.joinable()) thread_.join();
}
void GenerateCommitMessageThread::request(GenerativeAI::Model model, GenerativeAI::Credential credential,
                                        std::function<std::string(std::shared_ptr<std::atomic_bool>)> prepare, std::string hint)
{
	{
		std::lock_guard lock(mutex_);
		if (interrupted_) return;
		if (active_) active_->store(true);
		pending_ = Job{std::move(model), std::move(credential), std::move(prepare), std::move(hint), std::make_shared<std::atomic_bool>(false)};
	}
	cv_.notify_one();
}
