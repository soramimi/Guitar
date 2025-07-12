#include "GenerativeAI.h"
#include "common/misc.h"
#include "common/strformat.h"
#include "urlencode.h"

#include <QRegularExpression>

namespace GenerativeAI {

const std::vector<ProviderInfo> &provider_table()
{
	static const std::vector<ProviderInfo> provider_info = {
		{AI::Unknown, "-", "-", ""},
		{AI::OpenAI, "openai", "OpenAI; GPT", "OPENAI_API_KEY"},
		{AI::Anthropic, "anthropic", "Anthropic; Claude", "ANTHROPIC_API_KEY"},
		{AI::Google, "google", "Google; Gemini", "GOOGLE_API_KEY"},
		{AI::DeepSeek, "deepseek", "DeepSeek", "DEEPSEEK_API_KEY"},
		{AI::OpenRouter, "openrouter", "OpenRouter", "OPENROUTER_API_KEY"},
		{AI::Ollama, "ollama", "Ollama (experimental)", ""},
		{AI::LMStudio, "lmstudio", "LM Studio (experimental)", ""}
	};
	return provider_info;
}

std::vector<Model> const &ai_model_presets()
{
	static const std::vector<Model> preset_models = {
		{AI::OpenAI, "gpt-4.1"},
		{AI::OpenAI, "o4-mini"},
		{AI::Anthropic, "claude-sonnet-4-20250514"},
		{AI::Anthropic, "claude-3-7-sonnet-latest"},
		{AI::Anthropic, "claude-3-5-haiku-20241022"},
		{AI::Google, "gemini-2.5-pro-exp-03-25"},
		{AI::Google, "gemini-2.5-flash-preview-04-17"},
		{AI::DeepSeek, "deepseek-chat"},
		{AI::OpenRouter, "openrouter:///anthropic/claude-3.7-sonnet"},
		{AI::Ollama, "ollama:///gemma3:27b"},
		{AI::LMStudio, "lmstudio:///meta-llama-3-8b-instruct"},
	};
	return preset_models;
}

const ProviderInfo *provider_info(AI ai)
{
	std::vector<ProviderInfo> const &vec = provider_table();
	for (auto const &p : vec) {
		if (p.provider == ai) {
			return &p;
		}
	}
	return &vec[0];
}


std::string Model::default_model()
{
	return "gpt-4.1";
}

Model::Model(AI provider, std::string const &model_uri)
{
	provider_info_ = provider_info(provider);
	parse_model(model_uri);
}

void Model::parse_model(const std::string &name)
{
	long_name_ = name;
	model_name_ = name;

	auto Parse = [&](std::string const &header, int port){
		if (misc::starts_with(model_name_, header)) {
			model_name_ = model_name_.substr(header.size());
			auto i = model_name_.find('/');
			if (i != std::string::npos) {
				host_ = model_name_.substr(0, i);
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
					port_ = std::to_string(port);
				}
				return true;
			}
		}
		return false;
	};

	if (Parse("ollama://", 11434)) return;
	if (Parse("lmstudio://", 1234)) return;
}

Model Model::from_name(std::string const &name)
{
	struct Item {
		AI provider;
		char const *regex;
		Item(AI ai, char const *re)
			: provider(ai), regex(re)
		{}
	};
	static const std::vector<Item> items = {
		{AI::OpenAI, "^gpt-"},
		{AI::Anthropic, "^claude-"},
		{AI::Google, "^gemini-"},
		{AI::DeepSeek, "^deepseek-"},
		{AI::Ollama, "^ollama://"},
		{AI::LMStudio, "^lmstudio://"},
		{AI::OpenRouter, "^openrouter://"},
		{AI::OpenAI, "^o[0-9]+"}
	};
	for (auto const &item : items) {
		QRegularExpression re(item.regex);
		if (re.match(QString::fromStdString(name)).hasMatch()) {
			return Model{item.provider, name};
		}
	}
	return {};
}

struct _MakeRequest : public GenerativeAI::AbstractVisitor<Request> {

	Model model_;
	Credential cred_;

	_MakeRequest(Model const &model, Credential const &cred)
		: model_(model)
		, cred_(cred)
	{}

	Request case_Unknown()
	{
		return {};
	}

	Request case_OpenAI()
	{
		Request r;
		r.endpoint_url = "https://api.openai.com/v1/chat/completions";
		r.model_name = model_.model_name();
		r.header.push_back("Authorization: Bearer " + cred_.api_key);
		return r;
	}

	Request case_Anthropic()
	{
		Request r;
		r.endpoint_url = "https://api.anthropic.com/v1/messages";
		r.model_name = model_.model_name();
		r.header.push_back("x-api-key: " + cred_.api_key);
		r.header.push_back("anthropic-version: 2023-06-01"); // ref. https://docs.anthropic.com/en/api/versioning
		return r;
	}

	Request case_Google()
	{
		Request r;
		r.endpoint_url = "https://generativelanguage.googleapis.com/v1beta/models/" + url_encode(model_.model_name()) + ":generateContent?key=" + cred_.api_key;;
		r.model_name = model_.model_name();
		return r;
	}

	Request case_DeepSeek()
	{
		Request r;
		r.endpoint_url = "https://api.deepseek.com/chat/completions";
		r.model_name = model_.model_name();
		r.header.push_back("Authorization: Bearer " + cred_.api_key);
		return r;
	}

	Request case_OpenRouter()
	{
		Request r;
		r.endpoint_url = "https://openrouter.ai/api/v1/chat/completions";
		r.model_name = model_.model_name();
		r.header.push_back("Authorization: Bearer " + cred_.api_key);
		return r;
	}

	Request case_Ollama()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint_url = strf("http://%s:%s/api/generate")(model_.host())(model_.port()); // experimental
		r.header.push_back("Authorization: Bearer anonymous"/* + cred_.api_key*/);
		return r;
	}

	Request case_LMStudio()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint_url = strf("http://%s:%s/v1/completions")(model_.host())(model_.port()); // experimental
		return r;
	}
};

Request make_request(AI provider, const Model &model, Credential const &cred)
{
	return _MakeRequest(model, cred).visit(provider);
}

} // namespace GenerativeAI
