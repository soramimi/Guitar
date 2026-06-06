#ifndef GENERATEDCOMMITMESSAGE_H
#define GENERATEDCOMMITMESSAGE_H

#include <QString>
#include <ai/CommitMessageGenerator.h>

class GeneratedCommitMessage {
	friend class CommitMessageGenerator;
private:
	std::shared_ptr<CommitMessageGenerator::CommitMessageGenerator::Result> result_;
public:
	GeneratedCommitMessage()
	{
		result_ = std::make_shared<CommitMessageGenerator::CommitMessageGenerator::Result>();
	}
	GeneratedCommitMessage(CommitMessageGenerator::CommitMessageGenerator::Result *p)
		: result_(p)
	{
	}
	explicit operator bool () const
	{
		return !result_->error;
	}
	std::vector<std::string> const &messages() const
	{
		return result_->messages;
	}
	std::string const &error_status() const
	{
		return result_->error_status;
	}
	std::string const &error_message() const
	{
		return result_->error_message;
	}
};
Q_DECLARE_METATYPE(GeneratedCommitMessage)

#endif // GENERATEDCOMMITMESSAGE_H
