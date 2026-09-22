#include <ai/GenerativeAI.h>
#include <common/urlencode.h>
#include <common/fmt.h>
#include <common/joinpath.h>
#include <common/misc.h>
#include <regex>

namespace GenerativeAI {

/**
 * @brief 既定のAIモデル名を返す。
 * @return デフォルトモデル名の文字列。
 */
std::string Model::default_model()
{
	return "claude-sonnet-5";
}

/**
 * @brief AIプロバイダの完全なマスターテーブルを返す。
 * @return 全プロバイダの情報を保持するベクタへの参照。
 * @note placeholder エントリは API_KEY を管理する都合上設けられた代理アイテムであり、
 *       実際のAPIエンドポイントには対応しない。
 */
const std::vector<ProviderInfo> &complete_provider_table()
{
	static const std::vector<ProviderInfo> provider_info = {
		// id                                      tag                                description                       env_name
		{ProviderID::Unknown,                      "other",                           "Other",                            ""},
		{ProviderID::OpenAI,                       "",                                "OpenAI",                           "OPENAI_API_KEY"}, // placeholder
		{ProviderID::OpenAI_responses,             "openai-responses",                "OpenAI / GPT (responses)",         "OPENAI_API_KEY"},
		{ProviderID::OpenAI_chat_completions,      "openai-chat-completions",         "OpenAI / GPT (chat completions)",  "OPENAI_API_KEY"},
		{ProviderID::Anthropic,                    "anthropic",                       "Anthropic / Claude",               "ANTHROPIC_API_KEY"},
		{ProviderID::Google,                       "google",                          "Google / Gemini",                  "GEMINI_API_KEY"},
		{ProviderID::DeepSeek,                     "deepseek",                        "DeepSeek",                         "DEEPSEEK_API_KEY"},
		{ProviderID::MoonshotAI,                   "moonshot",                        "Moonshot AI / Kimi",               "MOONSHOT_API_KEY"},
		{ProviderID::XAI,                          "xai",                             "xAI / Grok",                       "XAI_API_KEY"},
		{ProviderID::PFN,                          "pfn",                             "Preferred Networks / PLaMo",       "PFN_API_KEY"},
		{ProviderID::Sakura,                       "sakura",                          "Sakura AI Engine",                 "SAKURA_AI_API_KEY"},
		{ProviderID::OpenRouter,                   "openrouter",                      "OpenRouter",                       "OPENROUTER_API_KEY"},
		{ProviderID::OrcaRouter,                   "orcarouter",                      "OrcaRouter",                       "ORCAROUTER_API_KEY"},
		{ProviderID::Requesty,                     "requesty",                        "Requesty",                         "REQUESTY_API_KEY"},
		{ProviderID::Merge,                        "merge",                           "Merge Gateway",                    "MERGE_API_KEY"},
		{ProviderID::Ollama,                       "ollama",                          "Ollama",                           "OLLAMA_API_KEY"},
		{ProviderID::LMStudio,                     "lmstudio",                        "LM Studio",                        "LMSTUDIO_API_KEY"},
		{ProviderID::LLAMACPP,                     "llamacpp",                        "llama.cpp",                        "LLAMACPP_API_KEY"},
	};
	return provider_info;
}

ProviderID api_compatibility(ProviderID pid)
{
	switch (pid) {
	case ProviderID::OpenAI:
	case ProviderID::OpenAI_responses:
	case ProviderID::Anthropic:
	case ProviderID::Google:
		return pid;
	default:
		return ProviderID::OpenAI_chat_completions;
	}
}

/**
 * @brief ユーザー向けに提示するAIモデルのプリセットリストを返す。
 * @return プリセットモデルのベクタへの参照。
 */
std::vector<Model> const &ai_model_presets()
{
	static const std::vector<Model> preset_models = {
		{ProviderID::OpenAI_responses, "gpt-5.6-luna"},
		{ProviderID::Anthropic,        "claude-sonnet-5"},
		{ProviderID::Google,           "gemini-3.8-flash"},
		{ProviderID::DeepSeek,         "deepseek-v4-flash"},
		{ProviderID::MoonshotAI,       "kimi-k2.7-code"},
		{ProviderID::XAI,              "grok-latest"},
		{ProviderID::PFN,              "plamo-3.0-prime"},
		{ProviderID::Sakura,           "sakura:gpt-oss-120b"},
		{ProviderID::OpenRouter,       "openrouter:anthropic/claude-4.6-sonnet"},
		{ProviderID::OrcaRouter,       "orcarouter:deepseek/deepseek-v4-pro-free"},
		{ProviderID::Requesty,         "requesty:google/gemma-4-31b-it"},
		{ProviderID::Merge,            "merge:openai/gpt-5.6-luna"},
		{ProviderID::Ollama,           "ollama:///gemma4"},
		{ProviderID::LMStudio,         "lmstudio:///meta-llama-3-8b-instruct"},
		{ProviderID::LLAMACPP,         "llamacpp://localhost:8080/"},
	};
	return preset_models;
}

/**
 * @brief ユーザー向けに提示するAIプロバイダIDのリストを返す。
 * @note Unknown は含むが、placeholder エントリは含まない。
 * @return AIプロバイダIDのベクタへの参照。
 */
std::vector<ProviderID> const &ai_provider_id_list_for_present_to_users()
{
	static std::vector<ProviderID> providers = { // Unknownは必要。placeholderを含まない。
		ProviderID::Unknown,
		ProviderID::OpenAI_responses,
		ProviderID::OpenAI_chat_completions,
		ProviderID::Anthropic,
		ProviderID::Google,
		ProviderID::DeepSeek,
		ProviderID::MoonshotAI,
		ProviderID::XAI,
		ProviderID::PFN,
		ProviderID::Sakura,
		ProviderID::OpenRouter,
		ProviderID::OrcaRouter,
		ProviderID::Requesty,
		ProviderID::Merge,
		ProviderID::Ollama,
		ProviderID::LMStudio,
		ProviderID::LLAMACPP,
	};
	return providers;
}

/**
 * @brief モデル名の文字列パターンからModelオブジェクトを生成する。
 * @param name モデル名またはURIを表す文字列。
 * @return 対応するAIプロバイダに紐付いたModelオブジェクト。パターン不一致の場合は空のModelを返す。
 */
Model Model::from_name(std::string const &name)
{
	struct Item {
		ProviderID provider;
		char const *regex;
		Item(ProviderID ai, char const *re)
			: provider(ai), regex(re)
		{}
	};
	static const std::vector<Item> items = {
		{ProviderID::OpenAI_responses, "^gpt-"},
		{ProviderID::Anthropic, "^claude-"},
		{ProviderID::Google, "^gemini-"},
		{ProviderID::DeepSeek, "^deepseek-"},
		{ProviderID::MoonshotAI, "^kimi-"},
		{ProviderID::XAI, "^grok-"},
		{ProviderID::PFN, "^plamo-"},
		{ProviderID::Sakura, "^sakura:"},
		{ProviderID::OpenRouter, "^openrouter:"},
		{ProviderID::OrcaRouter, "^orcarouter:"},
		{ProviderID::Requesty, "^requesty:"},
		{ProviderID::Requesty, "^merge:"},
		{ProviderID::Ollama, "^ollama://"},
		{ProviderID::LMStudio, "^lmstudio://"},
		{ProviderID::LLAMACPP, "^llamacpp://"},
	};
	for (auto const &item : items) {
		std::regex re(item.regex);
		if (std::regex_search(name, re)) {
			return Model{item.provider, name};
		}
	}
	return {};
}

/**
 * @brief モデル名またはURIを解析し、ホスト・ポート・モデル名を設定する。
 * @param model_uri 解析対象のモデル名またはURI文字列。
 */
void Model::parse_model(const std::string &model_uri)
{
	model_uri_.string = model_uri;
	model_name_ = model_uri;

	{
		static constexpr std::string_view prefix = "sakura:";
		if (misc::starts_with(model_name_, prefix)) {
			model_name_ = model_name_.substr(prefix.size());
			return;
		}
	}

	auto Parse = [&](std::string const &prefix, int port){
		if (misc::starts_with(model_name_, prefix)) {
			port_ = port;
			model_name_ = model_name_.substr(prefix.size());
			auto i = model_name_.find('/');
			if (i != std::string::npos) {
				std::string addr = model_name_.substr(0, i);
				model_name_ = model_name_.substr(i + 1);
				if (addr.empty()) {
					host_ = "localhost";
				} else {
					auto j = addr.find(':');
					if (j != std::string::npos) {
						host_ = addr.substr(0, j);
						port_ = misc::toi<int>(addr.substr(j + 1));
					}
				}
				return true;
			}
		}
		return false;
	};

	if (Parse("openrouter:", 443)) return;
	if (Parse("orcarouter:", 443)) return;
	if (Parse("requesty:", 443)) return;
	if (Parse("merge:", 443)) return;
	if (Parse("ollama://", 11434)) return;
	if (Parse("lmstudio://", 1234)) return;
	if (Parse("llamacpp://", 8080)) return;
}

/**
 * @brief AIプロバイダIDに対応するプロバイダ情報を返す。
 * @param id 検索対象のAIプロバイダID。
 * @return 対応する ProviderInfo へのポインタ。見つからない場合は Unknown エントリを返す。
 */
ProviderInfo const *provider_info(ProviderID id)
{
	std::vector<ProviderInfo> const &vec = complete_provider_table();
	for (auto const &p : vec) {
		if (p.id == id) {
			return &p;
		}
	}
	return &vec[0]; // Unknown
}

/**
 * @brief AIプロバイダとモデルURIからModelオブジェクトを構築する。
 * @param provider AIプロバイダID。
 * @param model_uri モデルのURI文字列。
 */
Model::Model(ProviderID provider, std::string const &model_uri)
{
	provider_info_ = provider_info(provider);
	parse_model(model_uri);
}

struct _ApiBaseUrl : public AbstractVisitor<std::string> {
	Model model_;

	_ApiBaseUrl(Model const &model)
		: model_(model)
	{}

	std::string _open_ai()
	{
		return "https://api.openai.com/v1/";
	}
	std::string _generic_host_and_port()
	{
		std::string host = model_.host();
		if (host.empty()) {
			host = "localhost";
		}
		int port = model_.port();
		return fmt("http://%s:%d/")(host)(port);
	}

	std::string case_Unknown()
	{
		return {};
	}
	std::string case_OpenAI()
	{
		return _open_ai();
	}
	std::string case_OpenAI_responses()
	{
		return _open_ai();
	}
	std::string case_OpenAI_chat_completions()
	{
		return _open_ai();
	}
	std::string case_Anthropic()
	{
		return "https://api.anthropic.com/v1/";
	}
	std::string case_Google()
	{
		return "https://generativelanguage.googleapis.com/v1beta/";
	}
	std::string case_XAI()
	{
		return "https://api.x.ai/v1/";
	}
	std::string case_PFN()
	{
		return "https://api.platform.preferredai.jp/v1/";
	}
	std::string case_MoonshotAI()
	{
		return "https://api.moonshot.ai/v1/";
	}
	std::string case_Sakura()
	{
		return "https://api.ai.sakura.ad.jp/v1/";
	}
	std::string case_DeepSeek()
	{
		return "https://api.deepseek.com/";
	}
	std::string case_OpenRouter()
	{
		return "https://openrouter.ai/api/v1/";
	}
	std::string case_OrcaRouter()
	{
		return "https://api.orcarouter.ai/v1/";
	}
	std::string case_Requesty()
	{
		return "https://router.requesty.ai/v1/";
	}
	std::string case_Merge()
	{
		return "https://api-gateway.merge.dev/v1/";
	}
	std::string case_Ollama()
	{
		return _generic_host_and_port();
	}
	std::string case_LMStudio()
	{
		return _generic_host_and_port();
	}
	std::string case_LLAMACPP()
	{
		return _generic_host_and_port();
	}
};

struct _MakeRequest : public AbstractVisitor<Request> {
	Model model_;
	Credential cred_;

	_MakeRequest(Model const &model, Credential const &cred)
		: model_(model)
		, cred_(cred)
	{}

	void set_authorization_bearer_cred(Request *r, Credential const &cred)
	{
		if (!cred.api_key.empty()) {
			r->header.push_back("Authorization: Bearer " + cred.api_key);
		}
	}

	std::string _endpoint_base_url()
	{
		return _ApiBaseUrl(model_).visit(model_.provider_id());
	}

	std::string _standard_api_suffix()
	{
		switch (model_.api_compatibility()) {
		case ProviderID::OpenAI_responses:
			return "responses";
		case ProviderID::OpenAI_chat_completions:
			return "chat/completions";
		case ProviderID::Anthropic:
			return "messages";
		}
		return {};
	}
	std::string _generic_endpoint_url()
	{
		return _endpoint_base_url() / _standard_api_suffix();
	}
	
	
	Request case_Unknown()
	{
		return {};
	}

	Request case_OpenAI()
	{
		Request r;
		r.model_name = model_.model_name();
		set_authorization_bearer_cred(&r, cred_);
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url());
		return r;
	}

	Request case_OpenAI_responses()
	{
		return case_OpenAI();
	}

	Request case_OpenAI_chat_completions()
	{
		return case_OpenAI();
	}

	Request case_Anthropic()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url());
		r.header.push_back("x-api-key: " + cred_.api_key);
		r.header.push_back("anthropic-version: 2023-06-01"); // ref. https://docs.anthropic.com/en/api/versioning
		return r;
	}

	Request case_Google()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.url_ = _endpoint_base_url();
		// r.endpoint.suffix_ = "/models/" + url_encode(model_.model_name()) + ":generateContent?key=" + cred_.api_key;
		return r;
	}

	Request case_XAI()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url());
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_PFN()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url());
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_MoonshotAI()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url());
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_Sakura()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url());
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}

	Request case_DeepSeek()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url());
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}

	Request case_OpenRouter()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url());
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_OrcaRouter()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url());
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_Requesty()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url());
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_Merge()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url());
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_Ollama()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.url_ = _endpoint_base_url();
		r.endpoint.suffix_ = "api/generate";
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}

	Request case_LMStudio()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.url_ = _endpoint_base_url();
		r.endpoint.suffix_ = "v1/completions";
		return r;
	}

	Request case_LLAMACPP()
	{
		Request r;
		r.model_name = model_.model_name();
		if (r.model_name.empty())  {
			r.model_name = "default";
		}
		r.endpoint.url_ = _endpoint_base_url() / "v1";
		r.endpoint.suffix_ = _standard_api_suffix();
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
};

/**
 * @brief 指定されたAIプロバイダ・モデル・認証情報からAPIリクエスト情報を生成する。
 * @param provider AIプロバイダID。
 * @param model 使用するAIモデル。
 * @param cred APIキー等の認証情報。
 * @return 生成されたRequestオブジェクト。
 */
Request make_request(ProviderID provider, const Model &model, Credential const &cred)
{
	Request ret = _MakeRequest(model, cred).visit(provider);
	if (model.endpoint_url_override) {
		ret.endpoint.set_chat_endpoint_url(*model.endpoint_url_override);
	}
	return ret;
}

void EndPoint::set_chat_endpoint_url(const std::string &url)
{
	url_ = url;
	static constexpr std::string_view suffix_chat_completions = "/chat/completions";
	static constexpr std::string_view suffix_responses = "/responses";
	static constexpr std::string_view suffix_messages = "/messages";
	auto Split = [&](std::string_view suffix){
		if (misc::ends_with(url_, suffix)) {
			url_ = url_.substr(0, url_.size() - suffix.size());
			suffix_ = suffix;
			return true;
		}
		return false;
	};
	if (Split(suffix_chat_completions)) return;
	if (Split(suffix_responses)) return;
	if (Split(suffix_messages)) return;

	// google special case
	if (url_.find(".googleapis.com/") != std::string::npos) {
		auto i = url_.find("/models/");
		if (i != std::string::npos) {
			suffix_ = url_.substr(i);
			url_ = url_.substr(0, i);
		}
	}
}

std::string EndPoint::url_chat(Model const &model, Credential const &cred) const
{
	std::string url = url_;

	// google special case
	if (url.find(".googleapis.com/") != std::string::npos) {
		url = url / "models" / url_encode(model.model_name()) + ":generateContent";
		url += "?key=" + cred.api_key;
	} else {
		if (!suffix_.empty()) {
			url = url / suffix_;
		}
	}
	
	return url;
}

std::string EndPoint::url_models(Credential const &cred) const
{
	std::string url = url_ / "models";
	
	// google special case
	if (url.find(".googleapis.com/") != std::string::npos) {
		url += "?key=" + cred.api_key;
	}
	
	return url;
}

} // namespace GenerativeAI
