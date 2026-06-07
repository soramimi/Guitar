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
	return "claude-sonnet-4-6";
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
		{ProviderID::Unknown,                      "",                                "-",                              ""},
		{ProviderID::OpenAI,                       "",                                "OpenAI",                         "OPENAI_API_KEY"}, // placeholder
		{ProviderID::OpenAI_responses,             "openai-responses",                "OpenAI",                         "OPENAI_API_KEY"},
		{ProviderID::OpenAI_chat_completions,      "openai-chat-completions",         "OpenAI (legacy)",                "OPENAI_API_KEY"},
		{ProviderID::Anthropic,                    "anthropic",                       "Anthropic; Claude",              "ANTHROPIC_API_KEY"},
		{ProviderID::Google,                       "google",                          "Google; Gemini",                 "GOOGLE_API_KEY"},
		{ProviderID::XAI,                          "xai",                             "xAI; Grok",                      "XAI_API_KEY"},
		{ProviderID::Sakura,                       "sakura",                          "Sakura AI Engine",               "SAKURA_AI_API_KEY"},
		{ProviderID::DeepSeek,                     "deepseek",                        "DeepSeek",                       "DEEPSEEK_API_KEY"},
		{ProviderID::OpenRouter,                   "openrouter",                      "OpenRouter",                     "OPENROUTER_API_KEY"},
		{ProviderID::Ollama,                       "ollama",                          "Ollama (experimental)",          ""},
		{ProviderID::LMStudio,                     "lmstudio",                        "LM Studio (experimental)",       ""},
		{ProviderID::LLAMACPP,                     "llamacpp",                        "llama.cpp (experimental)",       "LLAMACPP_API_KEY"},
	};
	return provider_info;
}

ProviderID api_compatibility(ProviderID pid)
{
	switch (pid) {
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
		{ProviderID::OpenAI_responses, "gpt-5.5"},
		{ProviderID::OpenAI_responses, "gpt-5.4-mini"},
		{ProviderID::OpenAI_responses, "gpt-5.4-nano"},
		{ProviderID::OpenAI_responses, "gpt-5.3-codex"},
		{ProviderID::Anthropic,        "claude-opus-4-7"},
		{ProviderID::Anthropic,        "claude-sonnet-4-6"},
		{ProviderID::Anthropic,        "claude-haiku-4-5"},
		{ProviderID::Google,           "gemini-3.1-flash-lite"},
		{ProviderID::XAI,              "grok-latest"},
		{ProviderID::Sakura,           "sakura:gpt-oss-120b"},
		{ProviderID::DeepSeek,         "deepseek-chat"},
		{ProviderID::OpenRouter,       "openrouter:///anthropic/claude-4.6-sonnet"},
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
		ProviderID::XAI,
		ProviderID::Sakura,
		ProviderID::DeepSeek,
		ProviderID::OpenRouter,
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
		{ProviderID::XAI, "^grok-"},
		{ProviderID::Sakura, "^sakura:"},
		{ProviderID::DeepSeek, "^deepseek-"},
		{ProviderID::Ollama, "^ollama://"},
		{ProviderID::LMStudio, "^lmstudio://"},
		{ProviderID::OpenRouter, "^openrouter://"},
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

	if (Parse("ollama://", 11434)) return;
	if (Parse("lmstudio://", 1234)) return;
	if (Parse("openrouter://", 80)) return;
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

	Request case_Unknown()
	{
		return {};
	}

	Request case_OpenAI()
	{
		Request r;
		r.model_name = model_.model_name();
		set_authorization_bearer_cred(&r, cred_);
		switch (model_.api_compatibility()) {
		case ProviderID::OpenAI_responses:
			r.endpoint = "https://api.openai.com/v1/responses";
			return r;
		case ProviderID::OpenAI_chat_completions:
			r.endpoint = "https://api.openai.com/v1/chat/completions";
			return r;
		}
		return {};
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
		r.endpoint = "https://api.anthropic.com/v1/messages";
		r.header.push_back("x-api-key: " + cred_.api_key);
		r.header.push_back("anthropic-version: 2023-06-01"); // ref. https://docs.anthropic.com/en/api/versioning
		return r;
	}

	Request case_Google()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint = "https://generativelanguage.googleapis.com/v1beta/models/" + url_encode(model_.model_name()) + ":generateContent?key=" + cred_.api_key;;
		return r;
	}

	Request case_XAI()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint = "https://api.x.ai/v1/chat/completions";
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}

	Request case_Sakura()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint = "https://api.ai.sakura.ad.jp/v1/chat/completions";
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}

	Request case_DeepSeek()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint = "https://api.deepseek.com/chat/completions";
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}

	Request case_OpenRouter()
	{
		Request r;
		r.endpoint = "https://openrouter.ai/api/v1/chat/completions";
		r.model_name = model_.model_name();
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}

	Request case_Ollama()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint = fmt("http://%s:%d/api/generate")(model_.host())(model_.port()); // experimental
		set_authorization_bearer_cred(&r, cred_);
		return r;
	}

	Request case_LMStudio()
	{
		Request r;
		r.model_name = model_.model_name();
		r.endpoint = fmt("http://%s:%d/v1/completions")(model_.host())(model_.port()); // experimental
		return r;
	}

	Request case_LLAMACPP()
	{
		Request r;
		r.model_name = model_.model_name();
		if (r.model_name.empty())  {
			r.model_name = "default";
		}
		switch (model_.api_compatibility()) {
		case ProviderID::Anthropic:
			r.endpoint = fmt("http://%s:%d/v1/messages")(model_.host())(model_.port());
			break;
		default:
			r.endpoint = fmt("http://%s:%d/v1/chat/completions")(model_.host())(model_.port()); // experimental
			break;
		}
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
	return _MakeRequest(model, cred).visit(provider);
}

/**
 * @brief モデルURLから環境変数名を生成する。
 *
 * 生成ルールは以下の通り：
 * - 英数字は大文字に変換する。
 * - その他の文字はアンダースコアに置換する。
 * - 連続する非英数字は単一のアンダースコアにまとめる。
 *
 * 例：
 * - "sakura:gpt-oss-120b" -> "SAKURA_GPT_OSS_120B"
 *
 * @param model_uri モデルURI
 * @return 環境変数名
 */
std::string makeEnvName(const ModelURI &model_uri)
{
	std::string ret;
	std::string const &s = model_uri.string;
	char last = 0;
	for (char c : s) {
		if (std::isalnum(c)) {
			ret += std::toupper(c);
		} else {
			c = '_';
			if (c != last) {
				ret += c;
			}
		}
		last = c;
	}
	return ret;
}

void EndPoint::operator = (const std::string &url)
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
}

std::string EndPoint::url_chat() const
{
	return suffix_.empty() ? url_ : (url_ / suffix_);
}

std::string EndPoint::url_models() const
{
	return url_ / "models";
}

} // namespace GenerativeAI
