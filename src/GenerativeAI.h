#ifndef GENERATIVEAI_H
#define GENERATIVEAI_H

#include <QString>
#include <vector>
#include <variant>

namespace GenerativeAI {

struct Unknown {
	std::string id() const { return "-"; }
	std::string description() const { return "-"; }
	std::string envname() const { return {}; }
};

struct OpenAI {
	std::string id() const { return "openai"; }
	std::string description() const { return "OpenAI; GPT"; }
	std::string envname() const { return "OPENAI_API_KEY"; }
};

struct Anthropic {
	std::string id() const { return "anthropic"; }
	std::string description() const { return "Anthropic; Claude"; }
	std::string envname() const { return "ANTHROPIC_API_KEY"; }
};

struct Google {
	std::string id() const { return "google"; }
	std::string description() const { return "Google; Gemini"; }
	std::string envname() const { return "GOOGLE_API_KEY"; }
};

struct DeepSeek {
	std::string id() const { return "deepseek"; }
	std::string description() const { return "DeepSeek"; }
	std::string envname() const { return "DEEPSEEK_API_KEY"; }
};

struct OpenRouter {
	std::string id() const { return "openrouter"; }
	std::string description() const { return "OpenRouter"; }
	std::string envname() const { return "OPENROUTER_API_KEY"; }
};

struct Ollama {
	std::string id() const { return "ollama"; }
	std::string description() const { return "Ollama"; }
	std::string envname() const { return {}; }
};

typedef std::variant<
	Unknown,
	OpenAI,
	Anthropic,
	Google,
	DeepSeek,
	OpenRouter,
	Ollama // experimental
	> Provider;

static inline std::vector<Provider> all_providers()
{
	return {
		Unknown{},
		OpenAI{},
		Anthropic{},
		Google{},
		DeepSeek{},
		OpenRouter{},
		// Ollama{} // experimental
	};
}

struct Credential {
	std::string api_key;
};

struct Model {
	Provider provider;
	std::string name;
	Model() = default;
	explicit Model(Provider const &provider, std::string const &name);
	void operator = (std::string const &name) = delete;

	static Model from_name(std::string const &name);
};

struct Request {
	std::string endpoint_url;
	std::string model;
	std::vector<std::string> header;
};

static inline std::string provider_id(Provider const &provider)
{
	return std::visit([](auto const &p) { return p.id(); }, provider);
}

static inline std::string provider_description(Provider const &provider)
{
	return std::visit([](auto const &p) { return p.description(); }, provider);
}

static inline std::string env_name(Provider const &provider)
{
	return std::visit([](auto const &p) { return p.envname(); }, provider);
}

Request make_request(Provider const &provider, Model const &model, Credential const &auth);

std::vector<Model> available_models();

} // namespace GenerativeAI

#endif // GENERATIVEAI_H
