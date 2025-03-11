#ifndef GENERATIVEAI_H
#define GENERATIVEAI_H

#include <QString>
#include <vector>
#include <variant>

namespace GenerativeAI {

enum Type {
	Unknown,
	GPT,
	CLAUDE,
	GEMINI,
	DEEPSEEK,
	OLLAMA,
	OPENROUTER,
};

struct OpenAI {
	std::string id() const { return "openai"; }
	std::string name() const { return "OpenAI; GPT"; }
	std::string envname() const { return "OPENAI_API_KEY"; }
	Type type() const { return GPT; }
};

struct Anthropic {
	std::string id() const { return "anthropic"; }
	std::string name() const { return "Anthropic; Claude"; }
	std::string envname() const { return "ANTHROPIC_API_KEY"; }
	Type type() const { return CLAUDE; }
};

struct Google {
	std::string id() const { return "google"; }
	std::string name() const { return "Google; Gemini"; }
	std::string envname() const { return "GOOGLE_API_KEY"; }
	Type type() const { return GEMINI; }
};

struct DeepSeek {
	std::string id() const { return "deepseek"; }
	std::string name() const { return "DeepSeek"; }
	std::string envname() const { return "DEEPSEEK_API_KEY"; }
	Type type() const { return DEEPSEEK; }
};

struct OpenRouter {
	std::string id() const { return "openrouter"; }
	std::string name() const { return "OpenRouter"; }
	std::string envname() const { return "OPENROUTER_API_KEY"; }
	Type type() const { return OPENROUTER; }
};

struct Ollama {
	std::string id() const { return "ollama"; }
	std::string name() const { return "Ollama"; }
	std::string envname() const { return {}; }
	Type type() const { return OLLAMA; }
};

typedef std::variant<
	OpenAI,
	Anthropic,
	Google,
	DeepSeek,
	OpenRouter, // experimental
	Ollama // experimental
	> Provider;

struct Credential {
	std::string api_key;
};

struct Model {
	Provider provider;
	std::string name;
	Model() = default;
	explicit Model(std::string const &name);
	void set(std::string const &name);
	void operator = (std::string const &name) = delete;
	Type type() const;
	QString anthropic_version() const;
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

static inline std::string provider_name(Provider const &provider)
{
	return std::visit([](auto const &p) { return p.name(); }, provider);
}

Request make_request(Provider const &provider, Model const &model, Credential const &auth);

std::vector<Model> available_models();

} // namespace GenerativeAI

#endif // GENERATIVEAI_H
