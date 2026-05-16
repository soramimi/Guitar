#ifndef AIAPIBRIDGE_H
#define AIAPIBRIDGE_H

#include "GenerativeAI.h"

#include <string>
#include <vector>

/// AIレスポンスの解析結果を保持する内部構造体
struct AiResult {
	bool completion = false;   ///< 正常に完了したか
	std::string content;          ///< AIが返したテキスト本文
	std::string error_status;  ///< エラー種別
	std::string error_message; ///< エラーメッセージ
};

class AiApiBridge {
public:
private:
	GenerativeAI::Model ai_model_;
	std::string generate_prompt_json(const GenerativeAI::Model &model, const std::string &prompt);
protected:
	virtual std::string generatePrompt() const = 0;
public:
	AiApiBridge();
	static AiResult Error(std::string const &status, std::string const &message)
	{
		AiResult ret;
		ret.error_status = status;
		ret.error_message = message;
		return ret;
	}
	GenerativeAI::Model ai_model();
	void set_ai_model(GenerativeAI::Model model);
	AiResult generate();
};

#endif // AIAPIBRIDGE_H
