#ifndef GENERATIVEAI_H
#define GENERATIVEAI_H

#include <string>
#include <vector>

namespace GenerativeAI {

enum class ProviderID {
	Unknown,
	OpenAI, // generic OpenAI placeholder
	OpenAI_responses, // for OpenAI responses API
	OpenAI_chat_completions, // legacy for OpenAI chat completions API
	Anthropic,
	Google,
	XAI,
	PFN,
	Moonshot,
	Sakura, // さくらの AI Engine（OpenAI chat completions 互換）
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
	virtual T case_OpenAI() {return {};} // placeholder
	virtual T case_OpenAI_responses() = 0;
	virtual T case_OpenAI_chat_completions() = 0;
	virtual T case_Anthropic() = 0;
	virtual T case_Google() = 0;
	virtual T case_XAI() = 0;
	virtual T case_PFN() = 0;
	virtual T case_Kimi() = 0;
	virtual T case_Sakura() = 0;
	virtual T case_DeepSeek() = 0;
	virtual T case_OpenRouter() = 0;
	virtual T case_Ollama() = 0;
	virtual T case_LMStudio() = 0;
	virtual T case_LLAMACPP() = 0;

	T visit(ProviderID provider)
	{
		switch (provider) {
		case ProviderID::Unknown:                 return case_Unknown();
		case ProviderID::OpenAI:                  return case_OpenAI();
		case ProviderID::OpenAI_responses:        return case_OpenAI_responses();
		case ProviderID::OpenAI_chat_completions: return case_OpenAI_chat_completions();
		case ProviderID::Anthropic:               return case_Anthropic();
		case ProviderID::Google:                  return case_Google();
		case ProviderID::XAI:                     return case_XAI();
		case ProviderID::PFN:                     return case_PFN();
		case ProviderID::Moonshot:                return case_Kimi();
		case ProviderID::Sakura:                  return case_Sakura();
		case ProviderID::DeepSeek:                return case_DeepSeek();
		case ProviderID::OpenRouter:              return case_OpenRouter();
		case ProviderID::Ollama:                  return case_Ollama();
		case ProviderID::LMStudio:                return case_LMStudio();
		case ProviderID::LLAMACPP:                return case_LLAMACPP();
		}
		return case_Unknown();
	}
};

struct ProviderInfo {
	ProviderID id; // 識別ID (整数)
	std::string tag; // 識別用タグ (小文字、ハイフン区切り)
	std::string description; // UI表示用説明文
	std::string env_name; // 環境変数名 (UPPER_SNAKE_CASE_API_KEY)
};

ProviderID api_compatibility(ProviderID pid);
std::vector<ProviderInfo> const &complete_provider_table();
const ProviderInfo *provider_info(ProviderID id);

struct Credential {
	std::string api_key;
};

class ModelURI {
public:
	std::string string;
	ModelURI() = default;
	explicit ModelURI(std::string const &s)
		: string(s)
	{
	}
};

struct Model {
	ModelURI model_uri_;
	ProviderInfo const *provider_info_;
	ProviderID api_compatibility__ = ProviderID::Unknown; // 基本的には設定しない。APIを選択できるプロバイダを使用する場合に指定できる。（例: llama.cppでAnthropic APIを使用する場合など）
	std::string model_name_;
	std::string host_;
	int port_ = 80;
	Model()
		: provider_info_(provider_info(ProviderID::Unknown))
	{}
	Model(ProviderID provider, const std::string &model_uri);
	void operator = (std::string const &) = delete;

	explicit operator bool () const
	{
		return provider_info_ && provider_info_->id != ProviderID::Unknown;
	}

	void parse_model(std::string const &model_uri);
	char const *reasoning_effort() const
	{
#if 0
		return "none";
		return "low";
		return "medium";
		return "high";
		return "xhigh";
#endif
		return nullptr; // default
	}

	ProviderID provider_id() const
	{
		return provider_info_ ? provider_info_->id : ProviderID::Unknown;
	}

	ModelURI model_uri() const
	{
		return model_uri_;
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

	std::string env_name() const
	{
		return provider_info_ ? provider_info_->env_name : "";
	}

	ProviderID api_compatibility() const
	{
		ProviderID pid = provider_id();
		return GenerativeAI::api_compatibility(pid);
	}

	static Model from_name(std::string const &name);
	static std::string default_model();
};

static inline bool operator == (ModelURI const &a, ModelURI const &b)
{
	return a.string == b.string;
}

struct EndPoint {
	enum class Type {
		None,
		Chat,
		Models,
	};
	std::string url_;
	std::string suffix_;
	EndPoint() = default;
	void operator = (std::string const &url);
	std::string url_chat() const;
	std::string url_models() const;
	std::string url(Type type)
	{
		switch (type) {
		case Type::Chat:
			return url_chat();
		case Type::Models:
			return url_models();
		}
		return url_;
	}
};

struct Request {
	EndPoint endpoint;
	std::string model_name;
	std::vector<std::string> header;
};

Request make_request(ProviderID provider, Model const &model, Credential const &auth);

std::vector<Model> const &ai_model_presets();
std::vector<GenerativeAI::ProviderID> const &ai_provider_id_list_for_present_to_users();

std::string makeEnvName(GenerativeAI::ModelURI const &model_uri);

} // namespace GenerativeAI

#endif // GENERATIVEAI_H
