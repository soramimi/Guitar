#ifndef COMMITMESSAGEGENERATOR_H
#define COMMITMESSAGEGENERATOR_H

#include "GenerativeAI.h"
#include <QObject>
#include <string>

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
Q_DECLARE_METATYPE(GeneratedCommitMessage)

class CommitMessageGenerator {
public:
	enum Kind {
		CommitMessage,
		DetailedComment,
	};
private:
	Kind kind;
	std::string error_status_;
	std::string error_message_;
	GeneratedCommitMessage parse_response(const std::string &in, GenerativeAI::Type ai_type);
	std::string generatePrompt(const QString &diff, int max);
	std::string generateDetailedPrompt(QString const &diff, const QString &commit_message);
	std::string generatePromptJSON(const std::string &prompt, const GenerativeAI::Model &model);
	GeneratedCommitMessage test();
public:
	CommitMessageGenerator() = default;
	CommitMessageGenerator(Kind kind)
		: kind(kind)
	{
	}
	GeneratedCommitMessage generate(const QString &diff, QString const &hint = {});
	std::string error() const
	{
		return error_message_;
	}
	static QString diff_head();
};

#endif // COMMITMESSAGEGENERATOR_H
