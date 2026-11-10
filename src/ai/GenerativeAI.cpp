#include <ai/GenerativeAI.h>
#include <common/urlencode.h>
#include <common/fmt.h>
#include <common/joinpath.h>
#include <common/misc.h>
#include <sys/stat.h>
#include <common/jstream.h>
#include <QDebug>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <set>
#include <regex>
#include <localvault/src/vault/SecureBuffer.h>
#include <localvault/src/gui/SecureStoreGUI.h>

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
		{ProviderID::Invalid,                      "invalid",                         "Invalid",                          ""},
		{ProviderID::Custom,                       "custom",                          "Custom",                           ""},
		{ProviderID::OpenAI,                       "openai",                          "OpenAI",                           "OPENAI_API_KEY"},
		{ProviderID::OpenAI_responses,             "openai-responses",                "OpenAI / GPT (responses)",         "OPENAI_API_KEY"},
		{ProviderID::OpenAI_chat_completions,      "openai-chat-completions",         "OpenAI / GPT (chat completions)",  "OPENAI_API_KEY"},
		{ProviderID::Anthropic,                    "anthropic",                       "Anthropic / Claude",               "ANTHROPIC_API_KEY"},
		{ProviderID::Google,                       "google",                          "Google / Gemini",                  "GEMINI_API_KEY"},
		{ProviderID::DeepSeek,                     "deepseek",                        "DeepSeek",                         "DEEPSEEK_API_KEY"},
		{ProviderID::MoonshotAI,                   "moonshot",                        "Moonshot AI / Kimi",               "MOONSHOT_API_KEY"},
		{ProviderID::XAI,                          "xai",                             "xAI / Grok",                       "XAI_API_KEY"},
		{ProviderID::PFN,                          "pfn",                             "Preferred Networks / PLaMo",       "PFN_API_KEY"},
		{ProviderID::Sakura,                       "sakura",                          "Sakura AI Engine",                 "SAKURAAI_API_KEY"},
		{ProviderID::Cloudflare,                   "cloudflare",                      "Cloudflare",                       "CLOUDFLARE_API_KEY"},
		{ProviderID::OpenRouter,                   "openrouter",                      "OpenRouter",                       "OPENROUTER_API_KEY"},
		{ProviderID::OrcaRouter,                   "orcarouter",                      "OrcaRouter",                       "ORCAROUTER_API_KEY"},
		{ProviderID::Requesty,                     "requesty",                        "Requesty",                         "REQUESTY_API_KEY"},
		{ProviderID::Merge,                        "merge",                           "Merge Gateway",                    "MERGE_API_KEY"},
		{ProviderID::Kilo,                         "kilo",                            "Kilo Gateway",                     "KILO_API_KEY"},
		{ProviderID::Ollama,                       "ollama",                          "Ollama",                           "OLLAMA_API_KEY"},
		{ProviderID::LMStudio,                     "lmstudio",                        "LM Studio",                        "LMSTUDIO_API_KEY"},
		{ProviderID::LLAMACPP,                     "llamacpp",                        "llama.cpp",                        "LLAMACPP_API_KEY"},
	};
	return provider_info;
}

ProviderID provider_id(std::string_view name)
{
	for (auto const &info : complete_provider_table()) {
		if (info.tag == name) {
			return info.id;
		}
	}
	return ProviderID::Invalid;
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

bool is_endpoint_customizable(ProviderID pid)
{
	switch (pid) {
	case ProviderID::Custom:
	case ProviderID::Ollama:
	case ProviderID::LMStudio:
	case ProviderID::LLAMACPP:
		return true;
	}
	return false;
}

/**
 * @brief ユーザー向けに提示するAIモデルのプリセットリストを返す。
 * @return プリセットモデルのベクタへの参照。
 */
std::vector<Model> const &ai_model_presets()
{
	static const std::vector<Model> preset_models = {
		{ProviderID::OpenAI_responses, "gpt-5.6-terra"},
		{ProviderID::Anthropic,        "claude-sonnet-5-5"},
		{ProviderID::Google,           "gemini-3.8-flash"},
		{ProviderID::DeepSeek,         "deepseek-flash"},
		{ProviderID::MoonshotAI,       "kimi-k2.7-code"},
		{ProviderID::XAI,              "grok-latest"},
		{ProviderID::PFN,              "plamo-3.0-prime"},
		{ProviderID::Sakura,           "sakura:gpt-oss-120b"},
		{ProviderID::Cloudflare,       "cloudflare:openai/gpt-6-luna"},
		{ProviderID::OpenRouter,       "openrouter:google/gemma-4-26b-a4b-it:free"},
		{ProviderID::OrcaRouter,       "orcarouter:deepseek/deepseek-v4.1-flash-free"},
		{ProviderID::Requesty,         "requesty:google/gemma-4-31b-it"},
		{ProviderID::Merge,            "merge:openai/gpt-5.6-terra"},
		{ProviderID::Kilo,             "kilo:anthropic/claude-sonnet-5.5"},
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
	static std::vector<ProviderID> providers = {
		ProviderID::Custom,
		ProviderID::OpenAI_responses,
		ProviderID::OpenAI_chat_completions,
		ProviderID::Anthropic,
		ProviderID::Google,
		ProviderID::DeepSeek,
		ProviderID::MoonshotAI,
		ProviderID::XAI,
		ProviderID::PFN,
		ProviderID::Sakura,
		ProviderID::Cloudflare,
		ProviderID::OpenRouter,
		ProviderID::OrcaRouter,
		ProviderID::Requesty,
		ProviderID::Merge,
		ProviderID::Kilo,
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
		{ProviderID::Cloudflare, "^cloudflare:"},
		{ProviderID::OpenRouter, "^openrouter:"},
		{ProviderID::OrcaRouter, "^orcarouter:"},
		{ProviderID::Requesty, "^requesty:"},
		{ProviderID::Requesty, "^merge:"},
		{ProviderID::Kilo, "^kilo:"},
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
			hostport_.port = port;
			model_name_ = model_name_.substr(prefix.size());
			if (auto i = model_name_.find("://"); i != std::string::npos) {
				std::string addr = model_name_.substr(0, i + 3);
				auto j = model_name_.find('/', i + 3);
				model_name_ = model_name_.substr(j + 1);
				if (addr.empty()) {
					hostport_.host = "localhost";
				} else {
					auto j = addr.find(':');
					if (j != std::string::npos) {
						hostport_.host = addr.substr(0, j);
						hostport_.port = misc::toi<int>(addr.substr(j + 1));
					}
				}
				return true;
			// } else if (auto j = model_name_.find('/'); i != std::string::npos) {
			// 	model_name_ = model_name_.substr(j + 1);
			}
			return true;
		}
		return false;
	};
	
	if (Parse("cloudflare:", 443)) return;
	if (Parse("openrouter:", 443)) return;
	if (Parse("orcarouter:", 443)) return;
	if (Parse("requesty:", 443)) return;
	if (Parse("merge:", 443)) return;
	if (Parse("kilo:", 443)) return;
	if (Parse("ollama://", 11434)) return;
	if (Parse("lmstudio://", 1234)) return;
	if (Parse("llamacpp://", 8080)) return;
}

HostPort parse_host_port(const std::string &url)
{
	HostPort ret;
	auto i = url.find("://");
	if (i != std::string::npos) {
		auto j = url.find('/', i + 3);
		if (j == std::string::npos) {
			j = url.size();
		}
		std::string sub = url.substr(i + 3, j - (i + 3));
		auto k = sub.find(':');
		if (k != std::string::npos) {
			ret.host = sub.substr(0, k);
			ret.port = misc::toi<int>(sub.substr(k + 1));
		} else {
			ret.host = sub;
		}
	}
	return ret;
}

void Model::set_endpoint_url(const std::string &url)
{
	endpoint_url_ = url;
	hostport_ = parse_host_port(url);
}

/**
 * @brief AIプロバイダIDに対応するプロバイダ情報を返す。
 * @param id 検索対象のAIプロバイダID。
 * @return 対応する ProviderInfo へのポインタ。見つからない場合は Invalid エントリを返す。
 */
ProviderInfo const *provider_info(ProviderID id)
{
	std::vector<ProviderInfo> const &vec = complete_provider_table();
	for (auto const &p : vec) {
		if (p.id == id) {
			return &p;
		}
	}
	return &vec[0]; // Invalid
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

	std::string case_Custom()
	{
		return _generic_host_and_port();
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
	std::string case_Cloudflare()
	{
		return "https://api.cloudflare.com/client/v4/accounts/{{ACCOUNT}}/ai/run";
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
	std::string case_Kilo()
	{
		return "https://ai-gateway.kilo.ai/api/v1/";
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
		case ProviderID::OpenAI: // fallthru
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
	
	
	Request case_Custom()
	{
		Request r;
		r.model_name = model_.model_name();
		set_authorization_bearer_cred(&r, cred_);
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
		return r;
	}

	Request case_OpenAI()
	{
		Request r;
		r.model_name = model_.model_name();
		set_authorization_bearer_cred(&r, cred_);
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
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
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
		r.header.push_back("x-api-key: " + cred_.api_key);
		r.header.push_back("anthropic-version: 2023-06-01"); // ref. https://docs.anthropic.com/en/api/versioning
		return r;
	}

	Request case_Google()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.url_ = _endpoint_base_url();
		return r;
	}

	Request case_XAI()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_PFN()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_MoonshotAI()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_DeepSeek()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_Sakura()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}

	Request case_Cloudflare()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.url_ = _endpoint_base_url();
		
		auto i = cred_.api_key.find(':');
		if (i != std::string::npos) {
			std::string key = cred_.api_key.substr(i + 1);
			r.header.push_back("Authorization: Bearer " + key);
		}
		return r;
	}

	Request case_OpenRouter()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_OrcaRouter()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_Requesty()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_Merge()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}
	
	Request case_Kilo()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint.set_chat_endpoint_url(_generic_endpoint_url(), cred_);
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
	if (!model.endpoint_url().empty()) {
		ret.endpoint.set_chat_endpoint_url(model.endpoint_url(), cred);
	}
	return ret;
}

static std::string rewrite_url_account(std::string url, Credential const &cred)
{
	static constexpr std::string_view account_placeholder = "{{ACCOUNT}}";
	std::string account;
	auto i = cred.api_key.find(':');
	if (i != std::string::npos) {
		account = cred.api_key.substr(0, i);
	}
	auto j = url.find(account_placeholder);
	if (j != std::string::npos) {
		url = url.substr(0, j) + account + url.substr(j + account_placeholder.size());
	}
	return url;
}

void EndPoint::set_chat_endpoint_url(const std::string &url, Credential const &cred)
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

	if (url_.find(".googleapis.com/") != std::string::npos) { // google special case
		auto i = url_.find("/models/");
		if (i != std::string::npos) {
			suffix_ = url_.substr(i);
			url_ = url_.substr(0, i);
		}
	} else if (url_.find(".cloudflare.com/") != std::string::npos) { // cloudflare special case
		url_ = rewrite_url_account(url_, cred);
	}
}

std::string EndPoint::url_chat(Model const &model, Credential const &cred, bool rewrite, std::optional<HostPort> hostport) const
{
	std::string url = url_;

	if (url.find(".googleapis.com/") != std::string::npos) { // google special case
		url = url / "models" / url_encode(model.model_name()) + ":generateContent";
		url += "?key=" + cred.api_key;
	} else {
		if (!suffix_.empty()) {
			url = url / suffix_;
		}
		if (rewrite) {
			if (url.find(".cloudflare.com/") != std::string::npos) { // cloudflare special case
				url = rewrite_url_account(url, cred);
			}
		}
	}

	if (hostport) {
		auto i = url.find("://");
		if (i != std::string::npos) {
			auto j = url.find('/', i + 3);
			if (j != std::string::npos) {
				// rewrite host and port
				std::string before = url.substr(0, i + 3);
				std::string middle = url.substr(i + 3, j - (i + 3));
				std::string after = url.substr(j);
				auto k = middle.find(':');
				if (k != std::string::npos) {
					middle = middle.substr(0, k);
				}
				if (!hostport->host.empty()) {
					middle = hostport->host;
				}
				std::string port_str;
				if (hostport->port > 0) {
					port_str = ":" + std::to_string(hostport->port);
				}
				url = before + middle + port_str + after;
			}
		}
	}

	return url;
}

std::string EndPoint::url_models(Credential const &cred, std::string const &cursor) const
{
	std::string url = url_ / "models";
	
	if (!cursor.empty()) {
		url += "?cursor=" + url_encode(cursor);
	}
	
	// google special case
	if (url.find(".googleapis.com/") != std::string::npos) {
		url += "?key=" + cred.api_key;
	}
	
	return url;
}


std::string api_type_name(ProviderID id)
{
	switch (id) {
	case ProviderID::OpenAI: case ProviderID::OpenAI_responses: return std::string(api_openai_responses_v1);
	case ProviderID::OpenAI_chat_completions: return std::string(api_openai_chat_completions_v1);
	case ProviderID::Anthropic: return std::string(api_anthropic_messages_v1);
	case ProviderID::Google: return std::string(api_google_gemini_v1);
	case ProviderID::Cloudflare: return std::string(api_cloudflare_gateway_v4);
	default: return {};
	}
}

std::optional<std::vector<ModelConf>> ModelConf::load(QWidget *, char const *path)
{
	QFile file(QString::fromUtf8(path));
	if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024) return std::nullopt;
	auto bytes = file.readAll();
	if (file.error() != QFile::NoError) return std::nullopt;
	QJsonParseError error;
	auto doc = QJsonDocument::fromJson(bytes, &error);
	if (error.error != QJsonParseError::NoError || !doc.isObject()) return std::nullopt;
	auto stored = doc.object().value("items");
	QJsonArray records;
	if (stored.isArray()) records = stored.toArray();
	else if (stored.isObject()) {
		// Older files used repeated "item" keys. Keep all records rather than
		// letting QJsonObject discard every record except the last one.
		jstream::Reader reader(bytes.constData(), bytes.size());
		bool valid = true;
		while (reader.next()) {
			if (reader.match_start_object("{items{item{**")) {
				QJsonObject record, credential;
				reader.nest([&] {
					for (auto key : {"guid", "name", "provider", "model", "endpoint_url", "api_type"}) {
						if (reader.match(std::string("@") + key)) {
							if (!reader.isstring()) valid = false;
							record.insert(key, QString::fromStdString(reader.string()));
						}
					}
					for (auto key : {"symbol", "method"}) {
						if (reader.match(std::string("@credential{") + key)) {
							if (!reader.isstring()) valid = false;
							credential.insert(key, QString::fromStdString(reader.string()));
						}
					}
				});
				record.insert("credential", credential); records.append(record);
			}
		}
		if (!valid || (!stored.toObject().isEmpty() && records.isEmpty())) return std::nullopt;
	} else return std::nullopt;
	std::vector<ModelConf> items;
	std::set<std::string> guids;
	for (auto value : records) {
		if (!value.isObject()) return std::nullopt;
		auto record = value.toObject();
		for (auto key : {"guid", "name", "provider", "model", "endpoint_url", "api_type"})
			if (record.contains(key) && !record.value(key).isString()) return std::nullopt;
		ModelConf conf;
		conf.guid = record.value("guid").toString().toStdString();
		if (conf.guid.empty() || !guids.insert(conf.guid).second) return std::nullopt;
		conf.name = record.value("name").toString().toStdString();
		auto provider = record.value("provider").toString().toStdString();
		auto id = provider.empty() ? ProviderID::Custom : provider_id(provider);
		if (id == ProviderID::Invalid) return std::nullopt;
		auto modelName = record.value("model").toString().toStdString();
		conf.model = Model(id, modelName);
		conf.model.model_name_ = modelName; // The stored field is a model ID, not a URI.
		conf.model.set_endpoint_url(record.value("endpoint_url").toString().toStdString());
		conf.api_type = record.value("api_type").toString().toStdString();
		if (conf.api_type.empty()) conf.api_type = api_type_name(id == ProviderID::Cloudflare ? id : conf.model.api_compatibility());
		auto api = parse_api_type(conf.api_type);
		if (api == ProviderID::Custom) return std::nullopt;
		conf.model.api_compatibility_override = api;
		if (record.contains("credential") && !record.value("credential").isObject()) return std::nullopt;
		auto credential = record.value("credential").toObject();
		for (auto key : {"symbol", "method"})
			if (credential.contains(key) && !credential.value(key).isString()) return std::nullopt;
		conf.api_key_symbol = credential.value("symbol").toString().toStdString();
		conf.api_key_method = credential.value("method").toString().toStdString();
		if (conf.api_key_method.empty()) conf.api_key_method = std::string(key_store_encryption);
		if (conf.api_key_method != key_store_environment && conf.api_key_method != key_store_encryption) return std::nullopt;
		items.push_back(std::move(conf));
	}
	return items;
}

std::optional<std::string> ModelConf::to_json(std::vector<ModelConf> const &items)
{
	QJsonArray records; std::set<std::string> guids;
	for (auto const &conf : items) {
		if (conf.guid.empty() || !guids.insert(conf.guid).second || !conf.model.provider_info_ || conf.model.provider_id() == ProviderID::Invalid) return std::nullopt;
		auto api = api_type_name(conf.model.api_compatibility());
		if (api.empty()) return std::nullopt;
		QJsonObject credential{{"symbol", QString::fromStdString(conf.api_key_symbol)}, {"method", QString::fromStdString(conf.api_key_method)}};
		records.append(QJsonObject{{"guid", QString::fromStdString(conf.guid)}, {"name", QString::fromStdString(conf.name)},
			{"provider", QString::fromStdString(conf.model.provider_info_->tag)}, {"api_type", QString::fromStdString(api)},
			{"model", QString::fromStdString(conf.model.model_name())}, {"endpoint_url", QString::fromStdString(conf.model.endpoint_url())}, {"credential", credential}});
	}
	// Preserve the established repeated-item format for older Guitar versions.
	jstream::Writer writer;
	writer.object({}, [&] {
		writer.object("items", [&] {
			for (auto value : records) {
				auto record = value.toObject();
				writer.object("item", [&] {
					for (auto key : {"guid", "name", "provider", "api_type", "endpoint_url", "model"}) writer.string(key, record.value(key).toString().toStdString());
					writer.object("credential", [&] {
						auto credential = record.value("credential").toObject();
						writer.string("symbol", credential.value("symbol").toString().toStdString());
						writer.string("method", credential.value("method").toString().toStdString());
					});
				});
			}
		});
	});
	std::string json = writer;
	if (json.size() > 4 * 1024 * 1024) return std::nullopt;
	return json;
}

bool ModelConf::save(QWidget *, char const *path, std::vector<ModelConf> const &items)
{
	auto json = to_json(items); if (!json) return false;
	QSaveFile file(QString::fromUtf8(path));
	return file.open(QIODevice::WriteOnly) && file.write(json->data(), json->size()) == static_cast<qint64>(json->size()) && file.commit();
}

} // namespace GenerativeAI
