#include "GenerativeAI.h"
#include "common/misc.h"
#include "common/strformat.h"
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
	models.emplace_back(Ollama{}, "ollama:///gemma3:27b"); // experimental
	return models;
}

Model::Model(const Provider &provider, const std::string &model_uri)
	: provider(provider)
{
	parse_model(model_uri);
}

void Model::parse_model(const std::string &name)
{
	long_name_ = name;
	model_name_ = name;

	if (misc::starts_with(model_name_, "ollama://")) {
		model_name_ = model_name_.substr(9);
		auto i = model_name_.find('/');
		if (i != std::string::npos) {
			host_ = model_name_.substr(0, i);
			port_;
			model_name_ = model_name_.substr(i + 1);
			if (host_.empty()) {
				host_ = "localhost";
			} else {
				auto j = model_name_.find(':');
				if (j != std::string::npos) {
					host_ = model_name_.substr(0, j);
					port_ = model_name_.substr(j + 1);
				}
			}
			if (port_.empty()) {
				port_ = "11434";
			}
		}
	}
}

Model Model::from_name(std::string const &name)
{
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
	if (misc::starts_with(name, "ollama://")) {
		return Model{Ollama{}, name.substr(9)};
	}
	if (name.find('/') != std::string::npos) {
		return Model{OpenRouter{}, name};
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
		r.model = model_.model_name();
		r.header.push_back("Authorization: Bearer " + cred_.api_key);
		return r;
	}

	Request operator () (Anthropic const &provider) const
	{
		Request r;
		r.endpoint_url = "https://api.anthropic.com/v1/messages";
		r.model = model_.model_name();
		r.header.push_back("x-api-key: " + cred_.api_key);
		r.header.push_back("anthropic-version: 2023-06-01"); // ref. https://docs.anthropic.com/en/api/versioning
		return r;
	}

	Request operator () (Google const &provider) const
	{
		Request r;
		r.endpoint_url = "https://generativelanguage.googleapis.com/v1beta/models/" + url_encode(model_.model_name()) + ":generateContent?key=" + cred_.api_key;;
		r.model = model_.model_name();
		return r;
	}

	Request operator () (DeepSeek const &provider) const
	{
		Request r;
		r.endpoint_url = "https://api.deepseek.com/chat/completions";
		r.model = model_.model_name();
		r.header.push_back("Authorization: Bearer " + cred_.api_key);
		return r;
	}

	Request operator () (OpenRouter const &provider) const
	{
		Request r;
		r.endpoint_url = "https://openrouter.ai/api/v1/chat/completions";
		r.model = model_.model_name();
		r.header.push_back("Authorization: Bearer " + cred_.api_key);
		return r;
	}

	Request operator () (Ollama const &provider) const
	{
		Request r;
		r.model = model_.model_name();
		r.endpoint_url = strformat("http://%s:%s/api/generate")(model_.host())(model_.port()); // experimental
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
