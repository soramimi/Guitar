#ifndef AIAPIBRIDGE_H
#define AIAPIBRIDGE_H

#include "GenerativeAI.h"

#include <string>
#include <vector>

/// AIレスポンスの解析結果を保持する内部構造体
struct AiResult {
	struct Data {
		bool completion = false;   ///< 正常に完了したか
		std::string content;          ///< AIが返したテキスト本文
		std::string error_status;  ///< エラー種別
		std::string error_message; ///< エラーメッセージ
	} d;

	operator bool () const
	{
		return d.completion || !d.error_status.empty() || !d.error_message.empty();
	}
	std::string content() const
	{
		return d.content;
	}
	std::string error_status() const
	{
		return d.error_status;
	}
	std::string error_message() const
	{
		return d.error_message;
	}
};

class AiApiBridge {
public:
private:
	GenerativeAI::Model ai_model_;
	std::string system_role_;
	std::string generate_prompt_json(const GenerativeAI::Model &model, const std::string &prompt, std::string const &system_role = {});
public:
	AiApiBridge();
	static AiResult Error(std::string const &status, std::string const &message)
	{
		AiResult ret;
		ret.d.error_status = status;
		ret.d.error_message = message;
		return ret;
	}
	GenerativeAI::Model ai_model();
	void set_ai_model(GenerativeAI::Model model);
	void set_system_role(std::string const &role);
	AiResult generate(GenerativeAI::EndPoint::Type eptype, std::string const &prompt);
	AiResult generate(const std::string &prompt);
};

#endif // AIAPIBRIDGE_H
