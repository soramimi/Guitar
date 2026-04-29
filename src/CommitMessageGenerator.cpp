#include "CommitMessageGenerator.h"
#include "ApplicationGlobal.h"
#include "common/jstream.h"
#include "common/joinpath.h"
#include "common/strformat.h"
#include "webclient.h"
#include "curlclient.h"
#include "GitRunner.h"
#include "Profile.h"
#include <QFile>
#include <QDebug>
#include "Logger.h"

/// AIレスポンスの解析結果を保持する内部構造体
struct CommitMessageResult {
	bool completion = false;   ///< 正常に完了したか
	std::string text;          ///< AIが返したテキスト本文
	std::string error_status;  ///< エラー種別
	std::string error_message; ///< エラーメッセージ
};

/**
 * @brief AIプロバイダーごとのJSONレスポンスを解析するビジタークラス。
 *
 * GenerativeAI::AbstractVisitor を継承し、プロバイダーの種類に応じた
 * JSONパス走査ロジックを各 case_* メソッドで実装する。
 */
struct _CommitMessageResponseParser : public GenerativeAI::AbstractVisitor<CommitMessageResult> {
	jstream::Reader reader;
	_CommitMessageResponseParser(std::string_view const &in)
		: reader(in.data(), in.data() + in.size())
	{}

	/**
	 * @brief OpenAI Chat Completions 形式のレスポンスを解析する。
	 *
	 * DeepSeek / OpenRouter / LM Studio / llama.cpp など、
	 * OpenAI 互換 API を使うプロバイダーで共通利用する。
	 * @return 解析結果
	 */
	CommitMessageResult parse_openai_chat_completions_format()
	{
		CommitMessageResult ret;
		while (reader.next()) {
			if (reader.match("{object")) {
				// "chat.completion" または "text_completion" なら正常完了
				if (reader.string() == "chat.completion" || reader.string() == "text_completion") {
					ret.completion = true;
				}
			} else if (reader.match("{choices[{message{content")) {
				ret.text = reader.string();
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
	CommitMessageResult case_Unknown()
	{
		return {};
	}

	/**
	 * @brief OpenAI Responses API 形式のレスポンスを解析する。
	 * @return 解析結果
	 */
	CommitMessageResult case_OpenAI_responses()
	{
		CommitMessageResult ret;
		while (reader.next()) {
			if (reader.match("{status")) {
				// status が "completed" であれば正常終了
				if (reader.string() == "completed") {
					ret.completion = true;
				}
			} else if (reader.match("{output[{content[{text")) {
				ret.text = reader.string();
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
	CommitMessageResult case_OpenAI_chat_completions()
	{
		return parse_openai_chat_completions_format();
	}

	/**
	 * @brief Anthropic Claude のレスポンスを解析する。
	 * @return 解析結果
	 */
	CommitMessageResult case_Anthropic()
	{
		CommitMessageResult ret;
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
				ret.text = reader.string();
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
	CommitMessageResult case_Google()
	{
		CommitMessageResult ret;
		while (reader.next()) {
			if (reader.match("{candidates[{content{parts[{text")) {
				ret.text = reader.string();
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

	/// xAISeek：OpenAI Chat Completions 互換形式
	CommitMessageResult case_XAI()
	{
		return parse_openai_chat_completions_format();
	}

	/// DeepSeek：OpenAI Chat Completions 互換形式
	CommitMessageResult case_DeepSeek()
	{
		return parse_openai_chat_completions_format();
	}

	/// OpenRouter：OpenAI Chat Completions 互換形式
	CommitMessageResult case_OpenRouter()
	{
		return parse_openai_chat_completions_format();
	}

	/**
	 * @brief Ollama のレスポンスを解析する。
	 *
	 * Ollama は独自フォーマットで、生成テキストが "response" キーに入る。
	 * @return 解析結果
	 */
	CommitMessageResult case_Ollama()
	{
		CommitMessageResult ret;
		while (reader.next()) {
			if (reader.match("{model")) {
				reader.string(); // モデル名は使用しないが読み捨てる
			} else if (reader.match("{response")) {
				ret.text = reader.string();
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
	CommitMessageResult case_LMStudio()
	{
		return parse_openai_chat_completions_format();
	}

	/// llama.cpp：OpenAI Chat Completions 互換形式
	CommitMessageResult case_LLAMACPP()
	{
		return parse_openai_chat_completions_format();
	}

};

/**
 * @brief AIプロバイダーごとのリクエストJSONを生成するビジタークラス。
 *
 * プロバイダー名とプロンプトを受け取り、各APIが要求するJSON形式に組み立てる。
 */
struct _PromptJsonGenerator : public GenerativeAI::AbstractVisitor<std::string> {
	constexpr static float temperature_ = 0.2f; ///< 応答のランダム性（Anthropic以外で使用）
	std::string modelname;
	std::string prompt;
	bool add_stream_false = false;
	_PromptJsonGenerator(std::string const &modelname, std::string const &prompt)
		: modelname(modelname)
		, prompt(prompt)
	{}

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
			w.string("model", modelname);
			w.number("temperature", temperature_);
			w.string("input", prompt);
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
			w.string("model", modelname);
			w.number("temperature", temperature_);
			w.array("messages", [&](){
				w.object({}, [&](){
					w.string("role", "system");
					w.string("content", "You are an experienced engineer.");
				});
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
			w.string("model", modelname);
			w.array("messages", [&](){
				w.object({}, [&](){
					w.string("role", "user");
					w.string("content", prompt);
				});
			});
			w.number("max_tokens", 200);
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
			w.string("model", modelname);
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
		return case_OpenAI_chat_completions();
	}
};

/**
 * @brief コンストラクタ。アプリ設定からAIモデルを初期化する。
 */
CommitMessageGenerator::CommitMessageGenerator()
{
	set_ai_model(global->appsettings.ai_model);
}

/**
 * @brief AIレスポンスのJSON文字列を解析してコミットメッセージ候補を取り出す。
 * @param in AIから返ってきたレスポンス文字列（JSON）
 * @param provider 使用したAIプロバイダー
 * @return コミットメッセージ候補のリスト、またはエラー情報
 */
CommitMessageGenerator::Result CommitMessageGenerator::parse_response(std::string const &in, GenerativeAI::AI provider)
{
	// プロバイダーに応じたパーサーでレスポンスを解析する
	CommitMessageResult r = _CommitMessageResponseParser(in).visit(provider);

	if (r.completion) {
		if (0) {
			// 旧実装：箇条書き形式（"- message" や "1. message"）をパースしていた。
			// 現在はJSON形式に移行したため使用しない。
			std::vector<std::string_view> lines = misc::splitLinesV(r.text, false);
			size_t i = lines.size();
			while (i > 0) {
				i--;
				std::string_view sv = lines[i];
				char const *ptr = sv.data();
				char const *end = ptr + sv.size();
				// 行頭・行末のバッククォートを除去
				while (ptr + 1 < end && *ptr == '`' && end[-1] == '`') {
					ptr++;
					end--;
				}
				bool accept = false;

				if (ptr < end && *ptr == '-') {
					// "- message" 形式
					accept = true;
					ptr++;
					while (ptr < end && (*ptr == '-' || isspace((unsigned char)*ptr))) { // e.g. "- - message"
						ptr++;
					}
				} else if (isdigit((unsigned char)*ptr)) {
					// "1. message" 形式
					while (ptr < end && isdigit((unsigned char)*ptr)) {
						accept = true;
						ptr++;
					}
					if (ptr < end && *ptr == '.') {
						ptr++;
					}
				}
				if (accept) {
					// 先頭の空白を除去
					while (ptr < end && isspace((unsigned char)*ptr)) {
						ptr++;
					}
					// 前後のダブルクォートを除去
					if (ptr + 1 < end && *ptr == '\"' && end[-1] == '\"') {
						ptr++;
						end--;
					}
					// 前後のアスタリスク（**bold**）を除去
					while (ptr + 1 < end && *ptr == '*' && end[-1] == '*') {
						ptr++;
						end--;
					}
					if (ptr < end) {
						// ok
					} else {
						accept = false;
					}
				}
				if (accept) {
					lines[i] = std::string_view(ptr, end - ptr);
				} else {
					lines.erase(lines.begin() + i);
				}
			}
			std::vector<std::string> ret;
			for (auto const &line : lines) {
				ret.emplace_back(line);
			}
			return ret;
		} else {
			// 現行実装：{"messages": ["msg1", "msg2", ...]} 形式のJSONを解析する
			std::vector<std::string> messages;
			std::string text = r.text;
			// JSONオブジェクトの範囲を前後から絞り込む（前後に余分なテキストが混入しても対応できるように）
			auto i = text.rfind('}');
			if (i != std::string::npos) {
				text = text.substr(0, i + 1);
			}
			auto j = text.find('{');
			if (j != std::string::npos) {
				text = text.substr(j);
			}
			jstream::Reader reader(text);
			while (reader.next()) {
				if (reader.match("{messages[") || reader.match("[")) {
					if (reader.isstring()) {
						messages.push_back(reader.string());
					}
				}
			}
			return messages;
		}
	} else {
		// AIがエラーを返した場合
		CommitMessageGenerator::Result ret;
		ret.error = true;
		ret.error_status = r.error_status;
		ret.error_message = r.error_message;
		if (ret.error_message.empty()) {
			ret.error_message = in; // エラー詳細が取れない場合はレスポンス全体を返す
		}
		return ret;
	}
}

/**
 * @brief diffからAIへ送るプロンプト文字列を生成する。
 *
 * AIにJSONフォーマット（{"messages": [...]}）で返すよう指示する。
 * @param diff コミット対象のdiff文字列
 * @param max 生成するコミットメッセージ候補の最大数
 * @return AIに送るプロンプト文字列
 */
std::string CommitMessageGenerator::generatePrompt(std::string const &diff, int max)
{
	return fmt(R"---(You are an experienced engineer.
Generate exactly %d concise git commit message candidates written in present tense for the following code diff.
Return ONLY valid and strict JSON. No explanations, no extra text.
Do NOT wrap the output in code fences (e.g., ``` or ```json).

/// Schema ///
{
  "messages": [
	"message1",
	"message2",
	"message3",
	...
  ]
}

/// git diff HEAD ///
%s
)---")(max)(diff);
}

/**
 * @brief プロンプトをプロバイダー固有のAPIリクエストJSON形式に変換する。
 * @param model 使用するAIモデル情報
 * @param prompt 送信するプロンプト文字列
 * @return APIリクエスト用JSON文字列
 */
std::string CommitMessageGenerator::generate_prompt_json(GenerativeAI::Model const &model, std::string const &prompt)
{
	return _PromptJsonGenerator(model.model_name(), prompt).visit(model.provider_id());
}

/**
 * @brief 現在設定されているAIモデルを返す。
 * @return AIモデル情報
 */
GenerativeAI::Model CommitMessageGenerator::ai_model()
{
	return ai_model_;
}

/**
 * @brief 使用するAIモデルを設定する。
 * @param model AIモデル情報
 */
void CommitMessageGenerator::set_ai_model(GenerativeAI::Model model)
{
	ai_model_ = model;
}

/**
 * @brief diff文字列を元にAIへリクエストを送り、コミットメッセージ候補を生成する。
 * @param diff コミット対象のdiff文字列
 * @return コミットメッセージ候補のリスト、またはエラー情報
 */
CommitMessageGenerator::Result CommitMessageGenerator::generate(std::string const &diff)
{
	constexpr int max_message_count = 5; // 生成するコミットメッセージ候補の数

	constexpr bool save_log = false; // リクエスト/レスポンスをログに記録するか

	if (diff.empty()) {
		return Error("error", "diff is empty");
	}

	// 巨大なdiffはトークン超過やコスト増加を招くため上限を設ける
	if (diff.size() > 100000) {
		return Error("error", "diff too large");
	}

	GenerativeAI::Model model = ai_model();
	if (model.provider_id() == GenerativeAI::AI::Unknown) {
		return Error("error", "AI model is not defined.");
	}

	std::string prompt = generatePrompt(diff, max_message_count);
	std::string json = generate_prompt_json(model, prompt);

	if (save_log) {
		logprintf(LOG_RAW, "%s\n", json.c_str());
	}

	// APIキーなどの認証情報を取得してリクエストヘッダーを組み立てる
	GenerativeAI::Credential cred = global->get_ai_credential(model.provider_id());
	GenerativeAI::Request ai_req = GenerativeAI::make_request(model.provider_id(), model, cred);

	InetClient::Request web_req;
	web_req.set_location(ai_req.endpoint_url);
	for (std::string const &h : ai_req.header) {
		web_req.add_header(h);
	}

	InetClient::Post post;
	post.content_type = "application/json";
	post.data.insert(post.data.end(), json.begin(), json.end());

	std::shared_ptr<AbstractInetClient> http = global->inet_client();
	if (http->post(web_req, &post)) {
		char const *data = http->content_data();
		size_t size = http->content_length();
		if (save_log) {
			std::string text(data, size);
			logprintf(LOG_RAW, "%s\n", text.c_str());
		}
		std::string text(data, size);
		CommitMessageGenerator::Result ret = parse_response(text, model.provider_id());
		return ret;
	}

	return {};
}

/**
 * @brief HEADとの差分を取得する内部実装。
 *
 * 各ファイルのdiffをスレッドで並列取得する。ただし libfile 内部の realloc が
 * クラッシュすることがあったため、現在はシングルスレッドで動作させている。
 * @param g GitRunner インスタンス
 * @param fn_accept ファイルをdiff対象に含めるか判定するコールバック
 * @return 連結されたdiff文字列
 */
static std::string diff_head(GitRunner g, std::function<bool (std::string const &name, std::string const &mime)> fn_accept)
{
	PROFILE;

	std::vector<std::string> names = g.diff_name_only_head();

	std::vector<std::string> diffs(names.size());
	const int NUM_THREADS = 1; // 8;
	// fileライブラリ内部のrealloc呼び出しがクラッシュすることがあったため
	// 安全のためシングルスレッドで動かす。(2025-12-25)
	std::vector<std::thread> threads(NUM_THREADS);
	std::atomic_size_t index(0);
	for (size_t t = 0; t < threads.size(); t++) {
		threads[t]= std::thread([&](GitRunner g){
			while (1) {
				size_t i = index++;
				if (i >= names.size()) break;
				std::string name = names[i];
				if (!name.empty()) {
					std::string file(g.workingDir() / name);
					std::string mimetype = global->determineFileType(file);
					if (misc::starts_with(mimetype, "image/")) continue; // 画像ファイルはdiffしない
					if (mimetype == "application/octetstream") continue; // バイナリファイルはdiffしない
					if (mimetype == "application/pdf") continue; // PDFはdiffしない
					if (fn_accept) {
						if (!fn_accept(file, mimetype)) continue; // ファイルの種類によるフィルタリング
					}
					diffs[i] = g.diff_full_index_head_file(file);
				}
			}
		}, g.dup());
	}

	std::string diff;

	for (size_t t = 0; t < threads.size(); t++) {
		threads[t].join();
	}

	// スレッドごとの結果を元のファイル順に連結する
	for (size_t i = 0; i < names.size(); i++) {
		if (!diffs[i].empty()) {
			diff += diffs[i];
		}
	}

	return diff;
}

/**
 * @brief HEADとのdiffを取得する（Guitar固有のフィルタを適用）。
 *
 * Qtの翻訳ファイル（*.ts）は行番号変化が多く差分がノイズになるため除外する。
 * @param g GitRunner インスタンス
 * @return フィルタ済みのdiff文字列
 */
std::string CommitMessageGenerator::diff_head(GitRunner g)
{
	std::string diff = ::diff_head(g, [&](std::string const &name, std::string const &mime) {
		if (mime == "text/xml" && misc::ends_with(name, ".ts")) return false; // Do not diff Qt translation TS files (line numbers and other changes are too numerous)
		return true;
	});
	return diff;
}
