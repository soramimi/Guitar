#ifndef GENERATIVEAI_H
#define GENERATIVEAI_H

#include <string>
#include <vector>

namespace GenerativeAI {

enum class AI {
	Unknown,
	OpenAI,
	Anthropic,
	Google,
	DeepSeek,
	OpenRouter,
	Ollama, // experimental
	LMStudio // experimental
};

template <typename T> class AbstractVisitor {
public:
	virtual ~AbstractVisitor() = default;

	virtual T case_Unknown() = 0;
	virtual T case_OpenAI() = 0;
	virtual T case_Anthropic() = 0;
	virtual T case_Google() = 0;
	virtual T case_DeepSeek() = 0;
	virtual T case_OpenRouter() = 0;
	virtual T case_Ollama() = 0;
	virtual T case_LMStudio() = 0;

	T visit(AI provider)
	{
		switch (provider) {
		case AI::Unknown:     return case_Unknown();
		case AI::OpenAI:      return case_OpenAI();
		case AI::Anthropic:   return case_Anthropic();
		case AI::Google:      return case_Google();
		case AI::DeepSeek:    return case_DeepSeek();
		case AI::OpenRouter:  return case_OpenRouter();
		case AI::Ollama:      return case_Ollama();
		case AI::LMStudio:    return case_LMStudio();
		}
		return case_Unknown();
	}
};

struct ProviderInfo {
	AI provider;
	std::string tag;
	std::string description;
	std::string env_name;
};

std::vector<ProviderInfo> const &provider_table();
ProviderInfo const *provider_info(AI ai);

struct Credential {
	std::string api_key;
};

struct Model {
	ProviderInfo const *provider_info_ = nullptr;
	std::string long_name_;
	std::string model_name_;
	std::string host_;
	std::string port_;
	Model()
		: provider_info_(provider_info(AI::Unknown))
	{}
	Model(AI provider, const std::string &model_uri);
	void operator = (std::string const &) = delete;

	void parse_model(std::string const &name);

	AI provider_id() const
	{
		return provider_info_ ? provider_info_->provider : AI::Unknown;
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

	std::string port() const
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
