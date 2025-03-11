#include "GenerativeAI.h"
#include "common/misc.h"
#include "urlencode.h"
#include "ApplicationGlobal.h"

namespace GenerativeAI {

std::vector<Model> available_models()
{
	std::vector<Model> models;
	models.emplace_back("gpt-4o");
	models.emplace_back("claude-3-7-sonnet-latest");
	models.emplace_back("gemini-2.0-flash");
	models.emplace_back("deepseek-chat");
	// models.emplace_back("ollama-llama3");
	models.emplace_back("openrouter-anthropic/claude-3.7-sonnet"); // experimental
	return models;
}

Model::Model(const std::string &name)
	: name(name)
{
	set(name);
}

void Model::set(std::string const &name)
{
	if (misc::starts_with(name, "gpt-")) {
		provider = OpenAI{};
		return;
	}
	if (misc::starts_with(name, "claude-")) {
		provider = Anthropic{};
		return;
	}
	if (misc::starts_with(name, "gemini-")) {
		provider = Google{};
		return;
	}
	if (misc::starts_with(name, "deepseek-")) {
		provider = DeepSeek{};
		return;
	}
	if (misc::starts_with(name, "ollama-")) {
		provider = Ollama{};
		return;
	}
	if (misc::starts_with(name, "openrouter-")) { // experimental
		provider = OpenRouter{};
		return;
	}
	provider = {};
}

Type Model::type() const
{
	if (misc::starts_with(name, "gpt-")) {
		return GPT;
	}
	if (misc::starts_with(name, "claude-")) {
		return CLAUDE;
	}
	if (misc::starts_with(name, "gemini-")) {
		return GEMINI;
	}
	if (misc::starts_with(name, "deepseek-")) {
		return DEEPSEEK;
	}
	if (misc::starts_with(name, "ollama-")) {
		return OLLAMA;
	}
	if (misc::starts_with(name, "openrouter-")) { // experimental
		return OPENROUTER;
	}
	return Unknown;
}

QString Model::anthropic_version() const
{
	return "2023-06-01"; // ref. https://docs.anthropic.com/en/api/versioning
}

struct Models {

	Model model_;
	Credential cred_;

	//

	Request operator () (OpenAI const &provider) const
	{
		Request r;
		r.endpoint_url = "https://api.openai.com/v1/chat/completions";
		r.model = model_.name;
		r.header.push_back("Authorization: Bearer " + cred_.api_key);
		return r;
	}

	Request operator () (Anthropic const &provider) const
	{
		Request r;
		r.endpoint_url = "https://api.anthropic.com/v1/messages";
		r.model = model_.name;
		r.header.push_back("x-api-key: " + cred_.api_key);
		r.header.push_back("anthropic-version: 2023-06-01"); // ref. https://docs.anthropic.com/en/api/versioning
		return r;
	}

	Request operator () (Google const &provider) const
	{
		Request r;
		r.endpoint_url = "https://generativelanguage.googleapis.com/v1beta/models/" + url_encode(model_.name) + ":generateContent?key=" + cred_.api_key;;
		r.model = model_.name;
		return r;
	}

	Request operator () (DeepSeek const &provider) const
	{
		Request r;
		r.endpoint_url = "https://api.deepseek.com/chat/completions";
		r.model = model_.name;
		r.header.push_back("Authorization: Bearer " + cred_.api_key);
		return r;
	}

	Request operator () (OpenRouter const &provider) const // experimental
	{
		Request r;
		r.endpoint_url = "https://openrouter.ai/api/v1/chat/completions";
		r.model = model_.name;
		r.header.push_back("Authorization: Bearer " + cred_.api_key);
		return r;
	}

	Request operator () (Ollama const &provider) const
	{
		Request r;
		r.endpoint_url = "http://localhost:11434/api/generate"; // experimental
		r.model = model_.name;
		r.header.push_back("Authorization: Bearer anonymous"/* + cred_.api_key*/);
		return r;
	}

	//

	Models(Model const &model, Credential const &cred)
		: model_(model)
		, cred_(cred)
	{
	}

	static Request make_request(Provider const &provider, Model const &model, Credential const &cred)
	{
		return std::visit(Models{model, cred}, provider);
	}
};

Request make_request(Provider const &provider, const Model &model, Credential const &cred)
{
	return Models::make_request(provider, model, cred);
}

} // namespace GenerativeAI
