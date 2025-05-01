#ifndef GENERATEDCOMMITMESSAGE_H
#define GENERATEDCOMMITMESSAGE_H

#include "CommitMessageGenerator.h"
#include <QString>

class GeneratedCommitMessage {
	friend class CommitMessageGenerator;
private:
	std::shared_ptr<CommitMessageGenerator::Result> result_;
public:
	GeneratedCommitMessage()
	{
		result_ = std::make_shared<CommitMessageGenerator::Result>();
	}
	GeneratedCommitMessage(CommitMessageGenerator::Result *p)
		: result_(p)
	{
	}
	operator bool () const
	{
		return !result_->error;
	}
	QStringList messages() const
	{
		return result_->messages;
	}
	QString error_status() const
	{
		return result_->error_status;
	}
	QString error_message() const
	{
		return result_->error_message;
	}
};
Q_DECLARE_METATYPE(GeneratedCommitMessage)

#endif // GENERATEDCOMMITMESSAGE_H
