#ifndef COMMITMESSAGEGENERATOR_H
#define COMMITMESSAGEGENERATOR_H

#include "GenerativeAI.h"
#include "Git.h"
#include <string>

class CommitMessageGenerator {
public:
	constexpr static int max_diff_size = 200000;

	struct CommitPair {
		std::string a = "HEAD";
		std::string b;
		CommitPair() = default;
		CommitPair(std::string const &a, std::string const &b)
			: a(a)
			, b(b)
		{
		}
	};


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
	GenerativeAI::Model ai_model_;
	CommitMessageGenerator::Result parse_response(GenerativeAI::Model model, const std::string &in);
	std::string generatePrompt(const std::string &diff, int max);
	std::string generate_prompt_json(const GenerativeAI::Model &model, const std::string &prompt);
	GenerativeAI::Model ai_model();
public:
	CommitMessageGenerator();
	Result generate(std::string const &diff);
	static Result Error(std::string const &status, std::string const &message)
	{
		Result ret;
		ret.error = true;
		ret.error_status = status;
		ret.error_message = message;
		return ret;
	}
	void set_ai_model(GenerativeAI::Model model);
	static bool accept_file_diff(const std::string &filename, const std::string &mimetype);
	static std::string make_diff(const std::string &gitcommand, const std::string &dir, CommitPair const &commits);
#ifdef APP_GUITAR
	static std::string make_diff(GitRunner g, CommitPair const &commits);
#endif
};

#endif // COMMITMESSAGEGENERATOR_H
