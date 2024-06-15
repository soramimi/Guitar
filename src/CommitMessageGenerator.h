#ifndef COMMITMESSAGEGENERATOR_H
#define COMMITMESSAGEGENERATOR_H

#include "Git.h"
#include <string>
#include <vector>

enum AI_Type {
	GPT,
	CLAUDE,
};

class CommitMessageGenerator {
private:
	std::string error_;
	std::vector<std::string> parse_openai_response(const std::string &in, AI_Type ai_type);
public:
	CommitMessageGenerator() = default;
	QStringList generate(GitPtr g);
	std::string error() const
	{
		return error_;
	}
};

#endif // COMMITMESSAGEGENERATOR_H
