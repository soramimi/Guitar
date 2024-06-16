#ifndef COMMITMESSAGEGENERATOR_H
#define COMMITMESSAGEGENERATOR_H

#include "GenerativeAI.h"
#include "Git.h"
#include <string>
#include <vector>

class GeneratedCommitMessage {
public:
	bool error = false;
	QString error_status;
	QString error_message;
	QStringList messages;
	GeneratedCommitMessage() = default;
	GeneratedCommitMessage(const QStringList &messages)
		: messages(messages)
	{
	}
	operator bool () const
	{
		return !error;
	}
	static GeneratedCommitMessage Error(QString status, QString message)
	{
		GeneratedCommitMessage ret;
		ret.error = true;
		ret.error_status = status;
		ret.error_message = message;
		return ret;
	}
};
Q_DECLARE_METATYPE(GeneratedCommitMessage);

class CommitMessageGenerator {
private:
	std::string error_status_;
	std::string error_message_;
	GeneratedCommitMessage parse_openai_response(const std::string &in, GenerativeAI::Type ai_type);
	std::string generatePrompt(QString diff, int max);
	std::string generatePromptJSON(const GenerativeAI::Model &model, QString diff, int max);
	GeneratedCommitMessage test();
public:
	CommitMessageGenerator() = default;
	GeneratedCommitMessage generate(GitPtr g);
	std::string error() const
	{
		return error_message_;
	}
};

#endif // COMMITMESSAGEGENERATOR_H
