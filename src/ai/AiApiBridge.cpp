#include <ai/AiApiBridge.h>
#include "Logger.h"
#include <ai/CommitMessageGenerator.h>
#include <common/jstream.h>
#include <inet/inetclient.h>

#ifdef APP_GUITAR
#include "ApplicationGlobal.h"
#endif

struct AiApiBridge::Private {
	GenerativeAI::Model ai_model;
	std::string system_role;
	std::shared_ptr<AbstractInetClient> http_;
	
	bool save_log = false; // リクエスト/レスポンスをログに記録するか
};

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
	: m(new Private)
{
	set_ai_model(global->appsettings.ai_model);
}
#else
GenerativeAI::Model global_appsettings_ai_model();
GenerativeAI::Credential global_get_ai_credential(GenerativeAI::Model const &model);
std::shared_ptr<AbstractInetClient> global_inet_client();

AiApiBridge::AiApiBridge()
	: m(new Private)
{
}
#endif

AiApiBridge::~AiApiBridge()
{
	delete m;
}

/**
 * @brief AIプロバイダーごとのJSONレスポンスを解析するビジタークラス。
 *
 * GenerativeAI::AbstractVisitor を継承し、プロバイダーの種類に応じた
 * JSONパス走査ロジックを各 case_* メソッドで実装する。
 */
struct AiChatResponseParser : public GenerativeAI::AbstractVisitor<AiResult> {
	GenerativeAI::Model model;
	jstream::Reader reader;
	AiChatResponseParser(GenerativeAI::Model model, std::string_view const &in)
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
		AiResult ret(model.api_compatibility());
		while (reader.next()) {
			if (reader.match("{id")) {
				ret.d.ex.id = reader.string();
			} else if (reader.match("{model")) {
				ret.d.ex.model = reader.string();
			} else if (reader.match("{object")) {
				// "chat.completion" または "text_completion" なら正常完了
				if (reader.string() == "chat.completion" || reader.string() == "text_completion") {
					ret.d.completed = true;
				}
			} else if (reader.match("{choices[**")) {
				reader.nest();
				AiResponseEx::OpenAiChoice choice;
				do {
					if (reader.match("{choices[{message{content")) {
						ret.d.content = reader.string();
					} else if (reader.match("{choices[{index")) {
						choice.index = reader.number();
					} else if (reader.match("{choices[{message{role")) {
						choice.message.role = reader.string();
					} else if (reader.match("{choices[{message{content")) {
						choice.message.content = reader.string();
					} else if (reader.match("{choices[{message{tool_calls[**")) {
						reader.nest();
						AiResponseEx::OpenAiChoice::Message::ToolCall toolcall;
						do {
							if (reader.match("{choices[{message{tool_calls[{id")) {
								toolcall.id = reader.string();
							} else if (reader.match("{choices[{message{tool_calls[{type")) {
								toolcall.type = reader.string();
							} else if (reader.match("{choices[{message{tool_calls[{function{name")) {
								toolcall.function.name = reader.string();
							} else if (reader.match("{choices[{message{tool_calls[{function{arguments")) {
								toolcall.function.arguments = reader.string();
							}
						} while (reader.next());
						choice.message.tool_calls.push_back(std::move(toolcall));
					} else if (reader.match("{choices[{finish_reason")) {
						choice.finish_reason = reader.string();
					}
				} while (reader.next());
				ret.d.ex.openai.choices.push_back(std::move(choice));
			} else if (reader.match("{choices[{message{content")) {
				ret.d.content = reader.string();
			} else if (reader.match("{error{type")) {
				ret.d.error_status = reader.string();
				ret.d.completed = false;
			} else if (reader.match("{error{message")) {
				ret.d.error_message = reader.string();
				ret.d.completed = false;
			}
		}
		return ret;
	}

	/// 未知プロバイダー：空の結果を返す
	AiResult case_Unknown()
	{
		return {model.api_compatibility()};
	}

	/**
	 * @brief OpenAI Responses API 形式のレスポンスを解析する。
	 * @return 解析結果
	 */
	AiResult case_OpenAI_responses()
	{
		AiResult ret(model.api_compatibility());
		ret = parse_responses(GenerativeAI::ProviderID::OpenAI_responses);
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
	AiResult parse_responses(GenerativeAI::ProviderID provider_id)
	{
		AiResult ret(model.api_compatibility());
		ret.d.ex.api_id = provider_id;
		while (reader.next()) {
			if (reader.match("{model")) {
				ret.d.ex.model = reader.string();
			} else if (reader.match("{id")) {
				ret.d.ex.id = reader.string();
			} else if (reader.match("{type")) {
				ret.d.ex.anthropic.type = reader.string();
				if (ret.d.ex.anthropic.type == "error") {
					ret.d.completed = false;
				}
			} else if (reader.match("{role")) {
				ret.d.ex.anthropic.role = reader.string();
			} else if (reader.match("{object")) {
				ret.d.ex.openai.object = reader.string();
			} else if (reader.match("{status")) {
				ret.d.ex.openai.status = reader.string();
				if (ret.d.ex.openai.status == "completed") {
					ret.d.completed = true;
				}
			} else if (reader.match_end_array("{content")) { // catch the end of content array
				std::string_view array_json = reader.extract(); // extract the entire array as JSON string
				assert(array_json.front() == '[' && array_json.back() == ']');
				if (!ret.d.ex.anthropic.content.empty()) {
					ret.d.ex.anthropic.content.back().content_json = array_json;
				}
			} else if (reader.match("{content[{**")) {
				reader.nest();
				AiResponseEx::AnthropicContentItem item;
				do {
					if (reader.match("{content[{type")) {
						item.type = reader.string();
					} else if (reader.match("{content[{text")) {
						item.text = reader.string();
					} else if (reader.match("{content[{id")) {
						item.id = reader.string();
					} else if (reader.match("{content[{name")) {
						item.name = reader.string();
					} else if (reader.match("{content[{caller{type")) {
						item.caller_type = reader.string();
					}
				} while (reader.next());
				ret.d.ex.anthropic.content.push_back(std::move(item));
			} else if (reader.match("{output[{**")) {
				reader.nest();
				AiResponseEx::OpenAiOutputItem item;
				do {
					if (reader.match("{output[{id")) {
						item.id = reader.string();
					} else if (reader.match("{output[{type")) {
						item.type = reader.string();
					} else if (reader.match("{output[{status")) {
						item.status = reader.string();
					} else if (reader.match("{output[{arguments")) {
						item.arguments = reader.string();
					} else if (reader.match("{output[{call_id")) {
						item.call_id = reader.string();
					} else if (reader.match("{output[{name")) {
						item.name = reader.string();
					} else if (reader.match("{output[{content[{text")) {
						item.content.push_back({ reader.string() });
					}
				} while (reader.next());
				ret.d.ex.openai.output.push_back(std::move(item));
			} else if (reader.match("{stop_reason")) {
				ret.d.ex.stop_reason = reader.string();
				if (ret.d.ex.stop_reason == "end_turn") {
					ret.d.completed = true;
				} else if (ret.d.ex.stop_reason == "tool_use") {
					ret.d.completed = true;
				} else {
					ret.d.completed = false;
					ret.d.error_status = ret.d.ex.stop_reason;
				}
			} else if (reader.match("{usage{input_tokens")) {
				ret.d.ex.usage.input_tokens = reader.number();
			} else if (reader.match("{usage{cache_creation_input_tokens")) {
				ret.d.ex.usage.cache_creation_input_tokens = reader.number();
			} else if (reader.match("{usage{cache_read_input_tokens")) {
				ret.d.ex.usage.cache_read_input_tokens = reader.number();
			} else if (reader.match("{usage{cache_creation{ephemeral_5m_input_tokens")) {
				ret.d.ex.usage.cache_creation.ephemeral_5m_input_tokens = reader.number();
			} else if (reader.match("{usage{cache_creation{ephemeral_1h_input_tokens")) {
				ret.d.ex.usage.cache_creation.ephemeral_1h_input_tokens = reader.number();
			} else if (reader.match("{usage{output_tokens")) {
				ret.d.ex.usage.output_tokens = reader.number();
			} else if (reader.match("{usage{service_tier")) {
				ret.d.ex.usage.service_tier = reader.string();
			} else if (reader.match("{usage{inference_geo")) {
				ret.d.ex.usage.inference_geo = reader.string();
			} else if (reader.match("{error{type")) {
				ret.d.ex.error.type = ret.d.error_status = reader.string();
				ret.d.completed = false;
			} else if (reader.match("{error{message")) {
				ret.d.ex.error.message = ret.d.error_message = reader.string();
				ret.d.completed = false;
			}
		}
		if (ret.d.ex.api_id == GenerativeAI::ProviderID::Anthropic) {
			if (!ret.d.ex.anthropic.content.empty()) {
				ret.d.content = ret.d.ex.anthropic.content.front().text;
			}
		} else if (ret.d.ex.api_id == GenerativeAI::ProviderID::OpenAI_responses) {
			if (!ret.d.ex.openai.output.empty()) {
				for (AiResponseEx::OpenAiOutputItem const &item1 : ret.d.ex.openai.output) {
					if (item1.status == "completed") {
						for (AiResponseEx::OpenAiOutputItem::Content const &item2 : item1.content) {
							ret.d.content += item2.text;
						}
					}
				}
			}
		}
		return ret;
	}
	
	AiResult case_Anthropic()
	{
		AiResult ret = parse_responses(GenerativeAI::ProviderID::Anthropic);
		return ret;
	}

	/**
	 * @brief Google Gemini のレスポンスを解析する。
	 * @return 解析結果
	 */
	AiResult case_Google()
	{
		AiResult ret(model.api_compatibility());
		while (reader.next()) {
			if (reader.match("{candidates[{content{parts[{text")) {
				ret.d.content = reader.string();
				ret.d.completed = true;
			} else if (reader.match("{candidates[{content{parts[**")) {
				AiResponseEx::GoogleContentItem item;
				reader.nest([&](){
					if (reader.match("{candidates[{content{parts[{functionCall{name")) {
						item.functionCall.name = reader.string();
					} else if (reader.match("{candidates[{content{parts[{functionCall{id")) {
						item.functionCall.id = reader.string();
					} else if (reader.match("{candidates[{content{parts[{text")) {
						item.functionCall.text = reader.string();
						ret.d.content += item.functionCall.text; // 累積して content に追加
					}
				});
				ret.d.ex.google.content_parts.push_back(item);
			} else if (reader.match_end_array("{candidates[{content{parts")) {
				std::string_view array_json = reader.extract(); // extract the entire array as JSON string
				if (!ret.d.ex.google.content_parts.empty()) {
					ret.d.ex.google.content_parts.back().functionCall.content_json = array_json;
				}
			} else if (reader.match("{candidates[{finishReason")) {
				ret.d.ex.stop_reason = reader.string();
				if (ret.d.ex.stop_reason == "STOP") {
					ret.d.completed = true;
				}
			} else if (reader.match("{error{message")) {
				ret.d.error_message = reader.string();
				ret.d.completed = false;
			} else if (reader.match("{error{status")) {
				ret.d.error_status = reader.string();
				ret.d.completed = false;
			}
		}
		return ret;
	}

	/// xAI：OpenAI Chat Completions 互換形式
	AiResult case_XAI()
	{
		return parse_openai_chat_completions_format();
	}
	
	/// PFN：OpenAI Chat Completions 互換形式
	AiResult case_PFN()
	{
		return parse_openai_chat_completions_format();
	}
	
	AiResult case_Kimi()
	{
		switch (model.api_compatibility()) {
		case GenerativeAI::ProviderID::Anthropic:
			return case_Anthropic();
		default:
			return parse_openai_chat_completions_format();
		}
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
		AiResult ret(model.api_compatibility());
		while (reader.next()) {
			if (reader.match("{model")) {
				reader.string(); // モデル名は使用しないが読み捨てる
			} else if (reader.match("{response")) {
				ret.d.content = reader.string();
				ret.d.completed = true;
			} else if (reader.match("{error{type")) {
				ret.d.error_status = reader.string();
				ret.d.completed = false;
			} else if (reader.match("{error{message")) {
				ret.d.error_message = reader.string();
				ret.d.completed = false;
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
	std::string system_role;
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
			if (model.provider_id() == GenerativeAI::ProviderID::Moonshot) {
				// pass
			} else {
				w.number("temperature", temperature_);
			}
			w.array("messages", [&](){
				if (!system_role.empty()) {
					w.object({}, [&](){
						w.string("role", "system");
						w.string("content", system_role);
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
	
	/// PFN：OpenAI Chat Completions 互換形式
	std::string case_PFN()
	{
		return case_OpenAI_chat_completions();
	}
	
	/// Kimi
	std::string case_Kimi()
	{
		switch (model.api_compatibility()) {
		case GenerativeAI::ProviderID::Anthropic:
			return case_Anthropic();
		default:
			return case_OpenAI_chat_completions();
		}
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
std::string AiApiBridge::generate_prompt_json(GenerativeAI::Model const &model, std::string const &prompt, std::string const &system_role)
{
	_PromptJsonGenerator generator(model, prompt);
	generator.system_role = system_role;
	return generator.visit(model.provider_id());
}

/**
 * @brief 現在設定されているAIモデルを返す。
 * @return AIモデル情報
 */
GenerativeAI::Model AiApiBridge::model() const
{
	return m->ai_model;
}

/**
 * @brief 使用するAIモデルを設定する。
 * @param model AIモデル情報
 */
void AiApiBridge::set_ai_model(GenerativeAI::Model model)
{
	m->ai_model = model;
}

void AiApiBridge::set_system_role(const std::string &role)
{
	m->system_role = role;
}

AiResult AiApiBridge::open()
{
	// constexpr GenerativeAI::EndPoint::Type eptype = GenerativeAI::EndPoint::Type::Chat;
	
	if (model().provider_id() == GenerativeAI::ProviderID::Unknown) {
		return Error("error", "AI model is not defined.");
	}
	
	m->http_ = global_inet_client();
	
	return {model().api_compatibility()};
}

void AiApiBridge::close()
{
	m->http_.reset();
}

AbstractInetClient *AiApiBridge::http()
{
	AbstractInetClient *ret = m->http_.get();
	assert(ret);
	return ret;
}

// experimental query for multi-turn conversation with tool use support
AiResult AiApiBridge::x_request(const Query2Request &req)
{
	if (!req) return {model().api_compatibility()};
	
	std::string request_json;
	if (req.type == Query2Request::TEXT) {
		request_json = generate_prompt_json(model(), req.prompt_text, m->system_role);
	} else if (req.type == Query2Request::JSON) {
		request_json = req.prompt_json;
	} else {
		return {model().api_compatibility()};
	}
	
	if (m->save_log) {
		logprintf(LOG_RAW, "%s\n", request_json.c_str());
	}
	
	int ret = -1;
	{
		InetClient::Request web_req;
		{
			// APIキーなどの認証情報を取得してリクエストヘッダーを組み立てる
			
			auto cred = global_get_ai_credential(model());
			auto aireq = GenerativeAI::make_request(model().provider_id(), model(), cred);
			
			web_req.set_location(aireq.endpoint.url(GenerativeAI::EndPoint::Type::Chat));
			for (std::string const &h : aireq.header) {
				web_req.add_header(h);
			}
		}
		{
			InetClient::Post post;
			post.content_type = "application/json";
			post.data.insert(post.data.end(), request_json.begin(), request_json.end());
			ret = http()->post(web_req, &post);
		}
	}
	
	std::string response_json;
	if (ret >= 0) {
		char const *data = http()->content_data();
		size_t size = http()->content_length();
		response_json.assign(data, size);
		if (m->save_log) {
			logprintf(LOG_RAW, "%s\n", response_json.c_str());
		}
		// fprintf(stderr, "%s\n", response_json.c_str());
	}
	if (response_json.empty()) {
		logprintf(LOG_DEFAULT, "No response received from AI API.\n");
		return {model().api_compatibility()};
	}
	
	AiResult result = AiChatResponseParser(model(), response_json).visit(model().api_compatibility());
	return result;
}

/**
 * @brief 指定されたエンドポイントタイプに対してプロンプトを送信し、AIの応答を取得する。
 *
 * @param eptype エンドポイントの種類（例: Chat, Modelsなど）
 * @param prompt 送信するプロンプト文字列
 * @param req 追加のリクエスト情報（将来的な拡張用）
 * @return AIからの応答を含むAiResultオブジェクト
 */
AiResult AiApiBridge::request(GenerativeAI::EndPoint::Type eptype, std::string const &prompt, Query2Request const &req)
{
	if (model().provider_id() == GenerativeAI::ProviderID::Unknown) {
		return Error("error", "AI model is not defined.");
	}
	
	std::string response_json;
	{
		std::string request_json = generate_prompt_json(model(), prompt, m->system_role);
		
		if (m->save_log) {
			logprintf(LOG_RAW, "%s\n", request_json.c_str());
		}
		
		InetClient::Request web_req;
		{
			// APIキーなどの認証情報を取得してリクエストヘッダーを組み立てる
			GenerativeAI::Credential cred = global_get_ai_credential(model());
			GenerativeAI::Request ai_req = GenerativeAI::make_request(model().provider_id(), model(), cred);
			
			web_req.set_location(ai_req.endpoint.url(eptype));
			for (std::string const &h : ai_req.header) {
				web_req.add_header(h);
			}
		}
		
		{
			std::shared_ptr<AbstractInetClient> http = global_inet_client();
			
			int ret = -1;
			if (eptype == GenerativeAI::EndPoint::Type::Chat) {
				InetClient::Post post;
				post.content_type = "application/json";
				post.data.insert(post.data.end(), request_json.begin(), request_json.end());
				ret = http->post(web_req, &post);
			} else if (eptype == GenerativeAI::EndPoint::Type::Models) {
				ret = http->get(web_req);
			} else {
				assert(0);
			}
			
			if (ret >= 0) {
				char const *data = http->content_data();
				size_t size = http->content_length();
				response_json.assign(data, size);
				if (m->save_log) {
					logprintf(LOG_RAW, "%s\n", response_json.c_str());
				}
				// fprintf(stderr, "%s\n", response_json.c_str());
			}
		}
	}
	
	
	if (eptype == GenerativeAI::EndPoint::Type::Chat) {
		return AiChatResponseParser(model(), response_json).visit(model().provider_id());
	}
	
	if (eptype == GenerativeAI::EndPoint::Type::Models) {
		AiResult ret{model().api_compatibility()};
		ret.d.completed = true;
		ret.d.content = response_json;
		return ret;
	}
	
	return {model().api_compatibility()};
}

AiResult AiApiBridge::request(std::string const &prompt)
{
	AiApiBridge::Query2Request req;
	return request(GenerativeAI::EndPoint::Type::Chat, prompt, req);
}

std::optional<AiResult::Models> AiApiBridge::queryModels()
{
	AiApiBridge::Query2Request req{GenerativeAI::EndPoint::Type::Models};
	AiResult result = request(GenerativeAI::EndPoint::Type::Models, {}, req);
	if (!result) return std::nullopt;

	AiResult::Models models;
	{
		jstream::Reader reader(result.content());
		while (reader.next()) {
			if (reader.match("{data[{**")) {
				reader.nest();
				AiResult::Model model;
				do {
					if (reader.match("{data[{id")) {
						model.id = reader.string();
					} else if (reader.match("{data[{object")) {
						model.object = reader.string();
					} else if (reader.match("{data[{created")) {
						model.created = reader.string();
					} else if (reader.match("{data[{owned_by")) {
						model.owned_by = reader.string();
					}
				} while (reader.next());
				models.list.push_back(model);
			}
		}
	}
	return models;
}


