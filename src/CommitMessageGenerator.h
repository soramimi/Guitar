#ifndef COMMITMESSAGEGENERATOR_H
#define COMMITMESSAGEGENERATOR_H

#include "GenerativeAI.h"
#include "Git.h"
#include <string>

class CommitMessageGenerator {
public:
	enum Kind {
		CommitMessage,
	};
	class Result {
	public:
		bool error = false;
		QString error_status;
		QString error_message;
		QStringList messages;
		Result() = default;
		Result(const QStringList &messages)
			: messages(messages)
		{
		}
	};
private:
	Kind kind;
	CommitMessageGenerator::Result parse_response(const std::string &in, const GenerativeAI::Provider &provider);
	std::string generatePrompt(const QString &diff, int max);
	std::string generatePromptJSON(const std::string &prompt, const GenerativeAI::Model &model);
public:
	CommitMessageGenerator() = default;
	CommitMessageGenerator(Kind kind)
		: kind(kind)
	{
	}
	Result generate(const QString &diff, QString const &hint = {});
	static std::string diff_head(GitRunner g);
	static Result Error(QString status, QString message)
	{
		Result ret;
		ret.error = true;
		ret.error_status = status;
		ret.error_message = message;
		return ret;
	}
};

#endif // COMMITMESSAGEGENERATOR_H
