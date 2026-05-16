#include "AiApiBridge.h"
#include "common/jstream.h"
#include "CommitMessageGenerator.h"
#include "Logger.h"
#include "inetclient.h"

#ifdef APP_GUITAR
#include "ApplicationGlobal.h"
#endif

/**
 * @brief コンストラクタ。アプリ設定からAIモデルを初期化する。
 */
#ifdef APP_GUITAR
GenerativeAI::Credential global_get_ai_credential(GenerativeAI::Model const &model)
{
	return global->get_ai_credential(model);
}
std::shared_ptr<AbstractInetClient> global_inet_client()
{
	return global->inet_client();
}

AiApiBridge::AiApiBridge()
{
	set_ai_model(global->appsettings.ai_model);
}
#else
GenerativeAI::Model global_appsettings_ai_model();
GenerativeAI::Credential global_get_ai_credential(GenerativeAI::Model const &model);
std::shared_ptr<AbstractInetClient> global_inet_client();

AiApiBridge::AiApiBridge()
{
}
#endif

/**
 * @brief AIプロバイダーごとのJSONレスポンスを解析するビジタークラス。
 *
 * GenerativeAI::AbstractVisitor を継承し、プロバイダーの種類に応じた
 * JSONパス走査ロジックを各 case_* メソッドで実装する。
 */
struct AiResponseParser : public GenerativeAI::AbstractVisitor<AiResult> {
	GenerativeAI::Model model;
	jstream::Reader reader;
	AiResponseParser(GenerativeAI::Model model, std::string_view const &in)
		: model(model)
		, reader(in.data(), in.data() + in.size())
	{}

	/**
	 * @brief OpenAI Chat Completions 形式のレスポンスを解析する。
	 *
	 * DeepSeek / OpenRouter / LM Studio / llama.cpp など、
	 * OpenAI 互換 API を使うプロバイダーで共通利用する。
	 * @return 解析結果
	 */
	AiResult parse_openai_chat_completions_format()
	{
		AiResult ret;
		while (reader.next()) {
			if (reader.match("{object")) {
				// "chat.completion" または "text_completion" なら正常完了
				if (reader.string() == "chat.completion" || reader.string() == "text_completion") {
					ret.completion = true;
				}
			} else if (reader.match("{choices[{message{content")) {
				ret.content = reader.string();
			} else if (reader.match("{error{type")) {
				ret.error_status = reader.string();
				ret.completion = false;
			} else if (reader.match("{error{message")) {
				ret.error_message = reader.string();
				ret.completion = false;
			}
		}
		return ret;
	}

	/// 未知プロバイダー：空の結果を返す
	AiResult case_Unknown()
	{
		return {};
	}

	/**
	 * @brief OpenAI Responses API 形式のレスポンスを解析する。
	 * @return 解析結果
	 */
	AiResult case_OpenAI_responses()
	{
		AiResult ret;
		while (reader.next()) {
			if (reader.match("{status")) {
				// status が "completed" であれば正常終了
				if (reader.string() == "completed") {
					ret.completion = true;
				}
			} else if (reader.match("{output[{content[{text")) {
				ret.content = reader.string();
			} else if (reader.match("{error")) {
				if (!reader.isnull()) {
					ret.error_status = reader.string();
					ret.completion = false;
				}
			}
		}
		return ret;
	}

	/// OpenAI Chat Completions 形式（共通実装に委譲）
	AiResult case_OpenAI_chat_completions()
	{
		return parse_openai_chat_completions_format();
	}

	/**
	 * @brief Anthropic Claude のレスポンスを解析する。
	 * @return 解析結果
	 */
	AiResult case_Anthropic()
	{
		AiResult ret;
		while (reader.next()) {
			if (reader.match("{stop_reason")) {
				// "end_turn" が正常終了を示す
				if (reader.string() == "end_turn") {
					ret.completion = true;
				} else {
					ret.completion = false;
					ret.error_status = reader.string();
				}
			} else if (reader.match("{content[{text")) {
				ret.content = reader.string();
			} else if (reader.match("{type")) {
				if (reader.string() == "error") {
					ret.completion = false;
				}
			} else if (reader.match("{error{type")) {
				ret.error_status = reader.string();
				ret.completion = false;
			} else if (reader.match("{error{message")) {
				ret.error_message = reader.string();
				ret.completion = false;
			}
		}
		return ret;
	}

	/**
	 * @brief Google Gemini のレスポンスを解析する。
	 * @return 解析結果
	 */
	AiResult case_Google()
	{
		AiResult ret;
		while (reader.next()) {
			if (reader.match("{candidates[{content{parts[{text")) {
				ret.content = reader.string();
				ret.completion = true;
			} else if (reader.match("{error{message")) {
				ret.error_message = reader.string();
				ret.completion = false;
			} else if (reader.match("{error{status")) {
				ret.error_status = reader.string();
				ret.completion = false;
			}
		}
		return ret;
	}

	/// xAI：OpenAI Chat Completions 互換形式
	AiResult case_XAI()
	{
		return parse_openai_chat_completions_format();
	}

	/// Sakura：OpenAI Chat Completions 互換形式
	AiResult case_Sakura()
	{
		return parse_openai_chat_completions_format();
	}


	/// DeepSeek：OpenAI Chat Completions 互換形式
	AiResult case_DeepSeek()
	{
		return parse_openai_chat_completions_format();
	}

	/// OpenRouter：OpenAI Chat Completions 互換形式
	AiResult case_OpenRouter()
	{
		return parse_openai_chat_completions_format();
	}

	/**
	 * @brief Ollama のレスポンスを解析する。
	 *
	 * Ollama は独自フォーマットで、生成テキストが "response" キーに入る。
	 * @return 解析結果
	 */
	AiResult case_Ollama()
	{
		AiResult ret;
		while (reader.next()) {
			if (reader.match("{model")) {
				reader.string(); // モデル名は使用しないが読み捨てる
			} else if (reader.match("{response")) {
				ret.content = reader.string();
				ret.completion = true;
			} else if (reader.match("{error{type")) {
				ret.error_status = reader.string();
				ret.completion = false;
			} else if (reader.match("{error{message")) {
				ret.error_message = reader.string();
				ret.completion = false;
			}
		}
		return ret;
	}

	/// LM Studio：OpenAI Chat Completions 互換形式
	AiResult case_LMStudio()
	{
		return parse_openai_chat_completions_format();
	}

	/// llama.cpp：OpenAI Chat Completions 互換形式
	AiResult case_LLAMACPP()
	{
		switch (model.api_compatibility()) {
		case GenerativeAI::ProviderID::Anthropic:
			return case_Anthropic();
		default:
			return parse_openai_chat_completions_format();
		}
	}
};

/**
 * @brief AIプロバイダーごとのリクエストJSONを生成するビジタークラス。
 *
 * プロバイダー名とプロンプトを受け取り、各APIが要求するJSON形式に組み立てる。
 */
struct _PromptJsonGenerator : public GenerativeAI::AbstractVisitor<std::string> {
	constexpr static float temperature_ = 0.2f; ///< 応答のランダム性（Anthropic以外で使用）
	constexpr static char const *system_role_ = "You are an experienced engineer."; ///< システムロールの内容（OpenAI Chat Completions 形式で使用）
	GenerativeAI::Model const &model;
	std::string prompt;
	bool add_stream_false = false;
	_PromptJsonGenerator(GenerativeAI::Model const &model, std::string const &prompt)
		: model(model)
		, prompt(prompt)
	{}

	std::string modelname() const
	{
		return model.model_name();
	}

	/// 未知プロバイダー：空文字列を返す
	std::string case_Unknown()
	{
		return {};
	}

	/**
	 * @brief OpenAI Responses API 向けのリクエストJSONを生成する。
	 * @return リクエストJSON文字列
	 */
	std::string case_OpenAI_responses()
	{
		jstream::Writer w;
		w.object({}, [&](){
			w.string("model", modelname());
			if (0) {
				w.number("temperature", temperature_); // deprecated
			}
			char const *reasoning_effort = model.reasoning_effort();
			if (reasoning_effort) {
				w.object("reasoning", [&](){
					w.string("effort", reasoning_effort);
				});
			}
			constexpr int format = 0;
			switch (format) {
			case 0:
				w.string("input", prompt);
				break;
			case 1:
				w.array("input", [&](){
					w.object({}, [&](){
						w.string("role", "user");
						w.string("content", prompt);
					});
				});
				break;
			}
		});
		return w;
	}

	/**
	 * @brief OpenAI Chat Completions API 向けのリクエストJSONを生成する。
	 * @return リクエストJSON文字列
	 */
	std::string case_OpenAI_chat_completions()
	{
		jstream::Writer w;
		w.object({}, [&](){
			w.string("model", modelname());
			w.number("temperature", temperature_);
			w.array("messages", [&](){
				if (system_role_) {
					w.object({}, [&](){
						w.string("role", "system");
						w.string("content", system_role_);
					});
				}
				w.object({}, [&](){
					w.string("role", "user");
					w.string("content", prompt);
				});
			});
			if (add_stream_false) {
				w.boolean("stream", false);
			}
		});
		return w;
	}

	/**
	 * @brief Anthropic Claude 向けのリクエストJSONを生成する。
	 *
	 * Claude Opus 4.7（2026-04-16リリース）以降、temperature / top_p / top_k は
	 * 非推奨となりデフォルト以外の値を送ると HTTP 400 が返るため省略する。
	 * @return リクエストJSON文字列
	 */
	std::string case_Anthropic()
	{
		jstream::Writer w;
		w.object({}, [&](){
			w.string("model", modelname());
			w.array("messages", [&](){
				w.object({}, [&](){
					w.string("role", "user");
					w.string("content", prompt);
				});
			});
			w.number("max_tokens", 1000);
			if (0) {
				// As of Claude Opus 4.7 (released 2026-04-16), temperature / top_p / top_k are deprecated.
				// Sending non-default values returns HTTP 400; omit these parameters entirely.
				// Control output via prompting or the effort parameter (low/medium/high/xhigh/max).
				// Ref: https://platform.claude.com/docs/en/about-claude/models/whats-new-claude-4-7
				w.number("temperature", temperature_);
			}
		});
		return w;
	}

	/**
	 * @brief Google Gemini 向けのリクエストJSONを生成する。
	 *
	 * Gemini API はモデル名をURLパラメータで指定するため、JSONに含まない。
	 * @return リクエストJSON文字列
	 */
	std::string case_Google()
	{
		jstream::Writer w;
		w.object({}, [&](){
			w.array("contents", [&](){
				w.object({}, [&](){
					w.array("parts", [&](){
						w.object({}, [&](){
							w.string("text", prompt);
						});
					});
				});
			});
		});
		return w;
	}

	/// xAI：OpenAI Chat Completions 互換形式
	std::string case_XAI()
	{
		return case_OpenAI_chat_completions();
	}

	/// Sakura：OpenAI Chat Completions 互換形式
	std::string case_Sakura()
	{
		return case_OpenAI_chat_completions();
	}

	/**
	 * @brief DeepSeek 向けのリクエストJSONを生成する。
	 *
	 * streamingを無効化している点がOpenAI互換形式と異なる。
	 * @return リクエストJSON文字列
	 */
	std::string case_DeepSeek()
	{
		add_stream_false = true; // OpenAI Chat Completions 形式のJSONに "stream": false を追加する
		return case_OpenAI_chat_completions();
	}

	/**
	 * @brief Ollama 向けのリクエストJSONを生成する。
	 *
	 * Ollama は独自フォーマットで prompt キーにプロンプトを渡す。
	 * @return リクエストJSON文字列
	 */
	std::string case_Ollama()
	{
		jstream::Writer w;
		w.object({}, [&](){
			w.string("model", modelname());
			w.string("prompt", prompt);
			w.boolean("stream", false);
		});
		return w;
	}

	/// OpenRouter：OpenAI Chat Completions 互換形式
	std::string case_OpenRouter()
	{
		return case_OpenAI_chat_completions();
	}

	/// LM Studio：Ollama 互換形式
	std::string case_LMStudio()
	{
		return case_Ollama();
	}

	/// llama.cpp：OpenAI Chat Completions 互換形式
	std::string case_LLAMACPP()
	{
		switch (model.api_compatibility()) {
		case GenerativeAI::ProviderID::Anthropic:
			return case_Anthropic();
		default:
			return case_OpenAI_chat_completions();
		}
	}
};

/**
 * @brief プロンプトをプロバイダー固有のAPIリクエストJSON形式に変換する。
 * @param model 使用するAIモデル情報
 * @param prompt 送信するプロンプト文字列
 * @return APIリクエスト用JSON文字列
 */
std::string AiApiBridge::generate_prompt_json(GenerativeAI::Model const &model, std::string const &prompt)
{
	return _PromptJsonGenerator(model, prompt).visit(model.provider_id());
}

/**
 * @brief 現在設定されているAIモデルを返す。
 * @return AIモデル情報
 */
GenerativeAI::Model AiApiBridge::ai_model()
{
	return ai_model_;
}

/**
 * @brief 使用するAIモデルを設定する。
 * @param model AIモデル情報
 */
void AiApiBridge::set_ai_model(GenerativeAI::Model model)
{
	ai_model_ = model;
}

/**
 * @brief diff文字列を元にAIへリクエストを送り、コミットメッセージ候補を生成する。
 * @param diff コミット対象のdiff文字列
 * @return コミットメッセージ候補のリスト、またはエラー情報
 */
AiResult AiApiBridge::generate()
{
	constexpr bool save_log = false; // リクエスト/レスポンスをログに記録するか

	GenerativeAI::Model model = ai_model();
	if (model.provider_id() == GenerativeAI::ProviderID::Unknown) {
		return Error("error", "AI model is not defined.");
	}

	std::string prompt = generatePrompt();
	std::string request_json = generate_prompt_json(model, prompt);

	if (save_log) {
		logprintf(LOG_RAW, "%s\n", request_json.c_str());
	}

	// APIキーなどの認証情報を取得してリクエストヘッダーを組み立てる
	GenerativeAI::Credential cred = global_get_ai_credential(model);
	GenerativeAI::Request ai_req = GenerativeAI::make_request(model.provider_id(), model, cred);

	InetClient::Request web_req;
	web_req.set_location(ai_req.endpoint_url);
	for (std::string const &h : ai_req.header) {
		web_req.add_header(h);
	}

	InetClient::Post post;
	post.content_type = "application/json";
	post.data.insert(post.data.end(), request_json.begin(), request_json.end());

	std::shared_ptr<AbstractInetClient> http = global_inet_client();
	if (http->post(web_req, &post)) {
		char const *data = http->content_data();
		size_t size = http->content_length();
		std::string response_json(data, size);
		if (save_log) {
			logprintf(LOG_RAW, "%s\n", response_json.c_str());
		}
		return AiResponseParser(ai_model(), response_json).visit(ai_model().provider_id());
	}

	return {};
}

