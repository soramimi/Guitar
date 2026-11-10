#ifndef COMMITMESSAGEGENERATOR_H
#define COMMITMESSAGEGENERATOR_H

#include "GitRunner.h"
#include <ai/AiApiBridge.h>
#include <string>
#include "ApplicationGlobal.h"

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
		api_.set_ai_model(model, global->get_ai_credential(model));
		api_.set_system_role("You are an experienced engineer.");
	}
	void set_ai_model(GenerativeAI::Model model, GenerativeAI::Credential cred)
	{
		api_.set_ai_model(model, cred);
	}
	
	void set_cancellation_token(std::shared_ptr<std::atomic_bool> token) { api_.set_cancellation_token(std::move(token)); }
	static Result parse_response(GenerativeAI::Model model, const AiResult &result);
	std::pair<std::string, std::string> generatePrompt() const;
	static bool accept_file_diff(const std::string &filename, const std::string &mimetype);
	static std::string make_diff(const std::string &gitcommand, const std::string &dir, CommitPair const &commits, std::shared_ptr<std::atomic_bool> cancelled = {});
#ifdef APP_GUITAR
	static std::string make_diff(GitRunner g, CommitPair const &commits, std::shared_ptr<std::atomic_bool> cancelled = {});
#endif

	AiResult request();
};

#endif // COMMITMESSAGEGENERATOR_H
