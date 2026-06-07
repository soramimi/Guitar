#ifndef COMMITMESSAGEGENERATOR_H
#define COMMITMESSAGEGENERATOR_H

#include <ai/AiApiBridge.h>
#include <ai/GenerativeAI.h>
#include "Git.h"
#include <string>

class CommitMessageGenerator {
private:
	AiApiBridge api_;
public:
	constexpr static int max_diff_size = 200000; // 適当

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

	struct Request {
		std::string diff;
		std::string status_s_u;
		std::string hint;
		int max_message_count = 5; // 生成するコミットメッセージ候補の数
		Request(std::string const &diff, std::string const &status_s_u, const std::string &hint)
			: diff(diff)
			, status_s_u(status_s_u)
			, hint(hint)
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

	CommitMessageGenerator::Request request_;
	CommitMessageGenerator(GenerativeAI::Model const &model, CommitMessageGenerator::Request const &request)
		: request_(request)
	{
		api_.set_ai_model(model);
		api_.set_system_role("You are an experienced engineer."); ///< システムロールの内容（OpenAI Chat Completions 形式で使用）
	}
	void set_ai_model(GenerativeAI::Model model)
	{
		api_.set_ai_model(model);
	}
	
	static Result parse_response(GenerativeAI::Model model, const AiResult &result);
	std::string generatePrompt() const;
	static bool accept_file_diff(const std::string &filename, const std::string &mimetype);
	static std::string make_diff(const std::string &gitcommand, const std::string &dir, CommitPair const &commits);
#ifdef APP_GUITAR
	static std::string make_diff(GitRunner g, CommitPair const &commits);
#endif

	AiResult request()
	{
		std::string prompt = generatePrompt();
		fprintf(stderr, "%s\n", prompt.c_str());
		fflush(stderr);
		return api_.request(prompt);
	}
};

#endif // COMMITMESSAGEGENERATOR_H
