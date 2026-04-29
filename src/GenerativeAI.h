#ifndef GENERATIVEAI_H
#define GENERATIVEAI_H

#include <string>
#include <vector>

namespace GenerativeAI {

enum class AI {
	Unknown,
	OpenAI, // generic OpenAI placeholder
	OpenAI_responses, // for OpenAI responses API
	OpenAI_chat_completions, // legacy for OpenAI chat completions API
	Anthropic,
	Google,
	XAI,
	DeepSeek,
	OpenRouter,
	Ollama, // experimental
	LMStudio, // experimental
	LLAMACPP, // experimental
};

template <typename T> class AbstractVisitor {
public:
	virtual ~AbstractVisitor() = default;

	virtual T case_Unknown() = 0;
	virtual T case_OpenAI_responses() = 0;
	virtual T case_OpenAI_chat_completions() = 0;
	virtual T case_Anthropic() = 0;
	virtual T case_Google() = 0;
	virtual T case_XAI() = 0;
	virtual T case_DeepSeek() = 0;
	virtual T case_OpenRouter() = 0;
	virtual T case_Ollama() = 0;
	virtual T case_LMStudio() = 0;
	virtual T case_LLAMACPP() = 0;

	T visit(AI provider)
	{
		switch (provider) {
		case AI::Unknown:                 return case_Unknown();
		case AI::OpenAI_responses:        return case_OpenAI_responses();
		case AI::OpenAI_chat_completions: return case_OpenAI_chat_completions();
		case AI::Anthropic:               return case_Anthropic();
		case AI::Google:                  return case_Google();
		case AI::XAI:                     return case_XAI();
		case AI::DeepSeek:                return case_DeepSeek();
		case AI::OpenRouter:              return case_OpenRouter();
		case AI::Ollama:                  return case_Ollama();
		case AI::LMStudio:                return case_LMStudio();
		case AI::LLAMACPP:                return case_LLAMACPP();
		}
		return case_Unknown();
	}
};

struct ProviderInfo {
	AI aiid; // 識別ID (整数)
	std::string tag; // 識別用タグ (小文字、ハイフン区切り)
	std::string description; // UI表示用説明文
	std::string symbol; // 設定ファイル用シンボル (PascalCase)
	std::string env_name; // 環境変数名 (UPPER_SNAKE_CASE_API_KEY)
};

std::vector<ProviderInfo> const &complete_provider_table();
const ProviderInfo *provider_info(AI aiid);

struct Credential {
	std::string api_key;
};

struct Model {
	ProviderInfo const *provider_info_;
	std::string long_name_;
	std::string model_name_;
	std::string host_;
	int port_ = 80;
	Model()
		: provider_info_(provider_info(AI::Unknown))
	{}
	Model(AI provider, const std::string &model_uri);
	void operator = (std::string const &) = delete;

	void parse_model(std::string const &name);

	AI provider_id() const
	{
		return provider_info_ ? provider_info_->aiid : AI::Unknown;
	}

	std::string long_name() const
	{
		return long_name_;
	}

	std::string model_name() const
	{
		return model_name_;
	}

	std::string host() const
	{
		return host_;
	}

	int port() const
	{
		return port_;
	}

	static Model from_name(std::string const &name);
	static std::string default_model();
};

struct Request {
	std::string endpoint_url;
	std::string model_name;
	std::vector<std::string> header;
};

Request make_request(AI provider, Model const &model, Credential const &auth);

std::vector<Model> const &ai_model_presets();

} // namespace GenerativeAI

#endif // GENERATIVEAI_H
