#include "GenerativeAI.h"
#include "common/misc.h"
#include "urlencode.h"
#include "ApplicationGlobal.h"

namespace GenerativeAI {

std::vector<Model> available_models()
{
	std::vector<Model> models;
	models.emplace_back(OpenAI{}, "gpt-4o");
	models.emplace_back(Anthropic{}, "claude-3-7-sonnet-latest");
	models.emplace_back(Google{}, "gemini-2.0-flash");
	models.emplace_back(DeepSeek{}, "deepseek-chat");
	models.emplace_back(OpenRouter{}, "anthropic/claude-3.7-sonnet");
	// models.emplace_back(Ollama{}, "ollama-gemma2"); // experimental
	return models;
}

Model::Model(const Provider &provider, const std::string &name)
	: provider(provider)
	, name(name)
{
}

Model Model::from_name(std::string const &name)
{
	if (name.find('/') != std::string::npos) {
		return Model{OpenRouter{}, name};
	}
	if (misc::starts_with(name, "gpt-")) {
		return Model{OpenAI{}, name};
	}
	if (misc::starts_with(name, "claude-")) {
		return Model{Anthropic{}, name};
	}
	if (misc::starts_with(name, "gemini-")) {
		return Model{Google{}, name};
	}
	if (misc::starts_with(name, "deepseek-")) {
		return Model{DeepSeek{}, name};
	}
	if (misc::starts_with(name, "ollama-")) {
		return Model{Ollama{}, name.substr(7)};
	}
	return {};
}



struct Models {

	Model model_;
	Credential cred_;

	//

	Request operator () (Unknown const &provider) const
	{
		return {};
	}

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

	Request operator () (OpenRouter const &provider) const
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
