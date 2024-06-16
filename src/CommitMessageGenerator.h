#ifndef COMMITMESSAGEGENERATOR_H
#define COMMITMESSAGEGENERATOR_H

#include "GenerativeAI.h"
#include "Git.h"
#include <string>
#include <vector>

class CommitMessageGenerator {
private:
	std::string error_;
	std::vector<std::string> parse_openai_response(const std::string &in, GenerativeAI::Type ai_type);
	std::string generatePrompt(QString diff, int max);
	std::string generatePromptJSON(const GenerativeAI::Model &model, QString diff, int max);
	std::vector<std::string> debug();
public:
	CommitMessageGenerator() = default;
	QStringList generate(GitPtr g);
	std::string error() const
	{
		return error_;
	}
};

#endif // COMMITMESSAGEGENERATOR_H
