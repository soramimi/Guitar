#ifndef COMMITMESSAGEGENERATOR_H
#define COMMITMESSAGEGENERATOR_H

#include "GenerativeAI.h"
#include "Git.h"
#include <string>

class CommitMessageGenerator {
public:
	class Result {
	public:
		bool error = false;
		std::string error_status;
		std::string error_message;
		std::vector<std::string> messages;
		Result() = default;
		Result(std::vector<std::string> const &messages)
			: messages(messages)
		{
		}
	};
private:
	CommitMessageGenerator::Result parse_response(const std::string &in, GenerativeAI::AI provider);
	std::string generatePrompt(const std::string &diff, int max);
	std::string generate_prompt_json(const GenerativeAI::Model &model, const std::string &prompt);
public:
	CommitMessageGenerator() = default;
	Result generate(std::string const &diff, QString const &hint = {});
	static std::string diff_head(GitRunner g);
	static Result Error(std::string const &status, std::string const &message)
	{
		Result ret;
		ret.error = true;
		ret.error_status = status;
		ret.error_message = message;
		return ret;
	}
};

#endif // COMMITMESSAGEGENERATOR_H
