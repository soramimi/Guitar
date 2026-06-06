
// cmms: Commit Message
//
// experimental code for generating commit messages using AI

#include <AiApiBridge.h>
#include <FileTypeDetector.h>
#include <common/ConfigParser.h>
#include <common/fmt.h>
#include <common/joinpath.h>
#include <common/q/FileInfo.h>
#include <common/selectitem.h>
#include <common/str.h>
#include <inet/curlclient.h>
// #include <inet/webclient.h>

#ifdef _WIN32
#include "common/wstring.h"
#include "process/ProcessWin.h"
#include <shlobj.h>
#include <string>
#include <windows.h>
#else
#include <process/ProcessPosix.h>

#include <jstream.h>
#endif

namespace misc {

std::string realpath(const char *path);
std::string realpath(std::string const &path);

}

static CurlContext curlcx;
// static WebContext webcx(WebClient::HTTP_1_1);
static GenerativeAI::Model ai_model;

struct Option {
	std::string config_file_path;
	std::string model_name;
	std::string prompt;
};

Option opt;

// AiApiBridgeからコールバックされる関数

void global_write_log(QString const &s)
{
	// nop:
}

GenerativeAI::Model global_appsettings_ai_model()
{
	return ai_model;
}

GenerativeAI::Credential global_get_ai_credential(GenerativeAI::Model const &model)
{
	GenerativeAI::Credential cred;
	std::string env_name = model.env_name();
	if (!env_name.empty()) {
		char const *env = std::getenv(env_name.c_str());
		if (env) {
			cred.api_key = env;
		} else {
			// fprintf(stderr, "Warning: Environment variable %s is not set. API key for %s will be empty.\n", env_name.c_str(), model.model_name().c_str());
		}
	}
	return cred;
}

std::shared_ptr<AbstractInetClient> global_inet_client()
{
	std::shared_ptr<AbstractInetClient> http;
	http = std::make_shared<CurlClient>(&curlcx);
	// http = std::make_shared<WebClient>(&webcx);
	return http;
}

//

FileTypeDetector file_type_detector;

std::string global_mimetype_by_file(std::string const &path)
{
	return file_type_detector.mimetype_by_file(path);
}

struct GitReturn {
	int exit_code = -1;
	std::string out_text;
	operator bool() const
	{
		return exit_code == 0;
	}
	operator std::string() const
	{
		return out_text;
	}
};

static std::string quoted_text(std::string const &str)
{
	std::string out = "\"";
	for (char c : str) {
		if (c == '\"') { // triple quotes
			out += "\"\"\"";
		} else {
			out += c;
		}
	}
	out += "\"";
	return out;
}

static std::vector<std::string_view> split_lines(std::string_view const &str)
{
	std::vector<std::string_view> ret;
	char const *begin = str.data();
	char const *end = begin + str.size();
	char const *ptr = begin;
	char const *left = ptr;
	while (1) {
		int c = 0;
		if (ptr < end) {
			c = (unsigned char)*ptr;
		}
		if (c == '\n' || c == '\r' || c == 0) {
			char const *right = ptr;
			if (c == '\n') {
				ptr++;
			} else if (c == '\r') {
				ptr++;
				if (ptr < end && *ptr == '\n') {
					ptr++;
				}
			}
			ret.emplace_back(left, right - left);
			if (c == 0) break;
			left = ptr;
		} else {
			ptr++;
		}
	}
	return ret;
}

struct Tool {
	constexpr static std::string_view use_prompt = "You say a quote of the day.";
	constexpr static std::string_view name = "get_quote_of_the_day";
	constexpr static std::string_view description = "Get a quote of the day";
	constexpr static std::string_view output = R"---({
    "quote": "The only way to do great work is to love what you do. - Steve Jobs"
})---";
};
Tool tool;

class ChatSession {
private:
	AiSession ai_;
	std::vector<AiApiBridge::Quert2Request> prompts_;
	size_t prompt_index_ = 0;
private:
	void add_tools(jstream::Writer *w)
	{
		auto apitype = ai_model.api_compatibility();
		w->array("tools", [&](){
			w->object({}, [&](){
				if (apitype == GenerativeAI::ProviderID::OpenAI_chat_completions) {
					w->string("type", "function");
					w->object("function", [&](){
						w->string("name", std::string{tool.name});
						w->string("description", tool.description);
						w->object("parameters", [&](){
							w->string("type", "object");
							w->object("properties", [&](){
							});
							w->array("required", [&](){
							});
						});
					});
				} else {
					if (apitype == GenerativeAI::ProviderID::OpenAI_responses) {
						w->string("type", "function");
					}
					w->string("name", std::string{tool.name});
					w->string("description", tool.description);
					w->object("input_schema", [&](){
						w->string("type", "object");
						w->object("properties", [&](){
						});
						w->array("required", [&](){
						});
					});
				}
			});
		});
	}
	AiApiBridge::Quert2Request make_chat_request(std::string const &text)
	{
		AiApiBridge::Quert2Request req;
		req.set_text(text);
		return req;
	}
	AiApiBridge::Quert2Request make_tooluse_request(std::string_view const &tool_use_prompt, std::string_view const &tool_name)
	{
		GenerativeAI::ProviderID apitype = ai_model.api_compatibility();
		jstream::Writer w;
		w.enable_indent(false);
		w.enable_newline(false);
		w.object({}, [&](){
			w.string("model", ai_model.model_name());
			if (apitype == GenerativeAI::ProviderID::Anthropic) {
				w.number("max_tokens", 1024);
			}
			add_tools(&w);
			w.object("tool_choice", [&](){
				if (apitype == GenerativeAI::ProviderID::Anthropic) {
					w.string("type", "auto");
					w.boolean("disable_parallel_tool_use", true);
				} else if (apitype == GenerativeAI::ProviderID::OpenAI_chat_completions) {
					w.string("type", "function");
					w.object("function", [&](){
						w.string("name", tool_name);
					});
				} else if (apitype == GenerativeAI::ProviderID::OpenAI_responses) {
					w.string("type", "function");
					w.string("name", tool_name);
				}
			});
			if (apitype == GenerativeAI::ProviderID::Anthropic || apitype == GenerativeAI::ProviderID::OpenAI_chat_completions) {
				w.array("messages", [&](){
					w.object({}, [&](){
						w.string("role", "user");
						w.string("content", tool_use_prompt);
					});
				});
			} else if (apitype == GenerativeAI::ProviderID::OpenAI_responses) {
				w.string("input", tool_use_prompt);
			}
		});
		AiApiBridge::Quert2Request req;
		req.set_tooluse(w, (std::string)tool_use_prompt);
		return req;
	}
public:
	void add_request(AiApiBridge::Quert2Request const &req)
	{
		prompts_.push_back(req);
	}
	void add_chat_request(std::string const &prompt)
	{
		add_request(make_chat_request(prompt));
	}
	void add_tool_use_request(std::string_view const &prompt, std::string_view const &function)
	{
		add_request(make_tooluse_request(prompt, function));
	}
private:
	void add_tool_result_request(std::string_view const &prompt, std::string const &json)
	{
		AiApiBridge::Quert2Request newreq;
		newreq.internal = true;
		newreq.set_tooluse(json, (std::string)prompt);
		prompts_.push_back(newreq);
	}
	bool tooluse_anthropic(AiResult const &msg)
	{
		if (msg.d.ex.anthropic.content.empty()) {
			fprintf(stderr, "Error: Unexpected response structure: anthropic.content is empty\n");
			return true;
		}

		size_t index = (size_t)-1;
		for (size_t i = 0; i < msg.d.ex.anthropic.content.size(); i++) {
			if (msg.d.ex.anthropic.content[i].type == "tool_use") {
				index = i;
				break;
			}
		}

		if (index != (size_t)-1) {
			std::string const &fname = msg.d.ex.anthropic.content[index].name;
			if (fname == tool.name) {
				jstream::Writer w;
				w.enable_indent(false);
				w.enable_newline(false);
				w.object({}, [&](){
					w.string("model", opt.model_name);
					w.number("max_tokens", 1024);
					add_tools(&w);
					w.object("tool_choice", [&](){
						w.string("type", "auto");
						w.boolean("disable_parallel_tool_use", true);
					});
					w.array("messages", [&](){
						w.object({}, [&](){
							w.string("role", "user");
							w.string("content", tool.use_prompt);
						});
						w.object({}, [&](){
							w.string("role", "assistant");
							w.raw("content", msg.d.ex.anthropic.content[index].content_json);
						});
						w.object({}, [&](){
							w.string("role", "user");
							w.array("content", [&](){
								w.object({}, [&](){
									w.string("type", "tool_result");
									w.string("tool_use_id", msg.d.ex.anthropic.content[index].id);
									w.string("content", tool.output);
								});
							});
						});
					});
				});
				add_tool_result_request(tool.use_prompt, w);
				return true;
			} else {
				fprintf(stderr, "Error: Unexpected tool name from Anthropic: '%s' (expected '%s')\n",
					fname.c_str(), tool.name.data());
				return true;
			}
		}
		return false;
	}
	bool tooluse_openai_responses(AiResult const &msg)
	{
		if (msg.d.ex.openai.output.empty()) {
			fprintf(stderr, "Error: Unexpected response structure: openai.output is empty\n");
			return true;
		}

		size_t index = (size_t)-1;
		for (size_t i = 0; i < msg.d.ex.openai.output.size(); i++) {
			if (msg.d.ex.openai.output[i].type == "function_call") {
				index = i;
				break;
			}
		}

		if (index != (size_t)-1) {
			std::string const &fname = msg.d.ex.openai.output[index].name;
			if (fname == tool.name) {
				jstream::Writer w;
				w.enable_indent(false);
				w.enable_newline(false);
				w.object({}, [&](){
					w.string("model", msg.d.ex.model);
					w.string("previous_response_id", msg.d.ex.id);
					w.array("input", [&](){
						w.object({}, [&](){
							w.string("type", "function_call_output");
							w.string("call_id", msg.d.ex.openai.output[index].call_id);
							w.string("output", tool.output);
						});
					});
					add_tools(&w);
				});
				add_tool_result_request(tool.use_prompt, w);
				return true;
			} else {
				fprintf(stderr, "Error: Unexpected function call from OpenAI responses: '%s' (expected '%s')\n",
					fname.c_str(), tool.name.data());
				return true;
			}
		}
		return false;
	}
	bool tooluse_openai_chat_completion(AiResult const &msg)
	{
		if (msg.d.ex.openai.choices.empty()) {
			fprintf(stderr, "Error: Unexpected response structure: openai.choices is empty\n");
			return true;
		}

		size_t index = (size_t)-1;
		for (size_t i = 0; i < msg.d.ex.openai.choices.size(); i++) {
			std::string finish_reason = msg.d.ex.openai.choices[i].finish_reason;
			if (finish_reason == "tool_calls" || finish_reason == "stop") {
				// "stop" can also indicate tool calls if the tool call is the reason for stopping
				// e.g. Kimi-K2.6のとき finish_reason == "stop" でここにくる
				index = i;
				break;
			}
		}

		if (index != (size_t)-1) {
			bool handled = false;
			using ToolCall = AiResponseEx::OpenAiChoice::Message::ToolCall;
			std::vector<ToolCall> const &tool_calls = msg.d.ex.openai.choices[index].message.tool_calls;
			for (ToolCall const &call : tool_calls) {
				if (call.type == "function" && call.function.name == tool.name) {
					jstream::Writer w;
					w.enable_indent(false);
					w.enable_newline(false);
					w.object({}, [&](){
						w.string("model", msg.d.ex.model);
						w.array("messages", [&](){
							w.object({}, [&](){
								w.string("role", "user");
								w.string("content", tool.use_prompt);
							});
							w.object({}, [&](){
								w.string("role", "assistant");
								w.null("content");
								w.array("tool_calls", [&](){
									w.object({}, [&](){
										w.string("id", call.id);
										w.string("type", call.type);
										w.object("function", [&](){
											w.string("name", call.function.name);
											w.string("arguments", call.function.arguments);
										});
									});
								});
							});
							w.object({}, [&](){
								w.string("role", "tool");
								w.string("tool_call_id", call.id);
								w.string("name", call.function.name);
								w.string("content", tool.output);
							});
						});
					});
					add_tool_result_request(tool.use_prompt, w);
					handled = true;
				} else {
					// モデルによってはツールの実行結果を'stop'かつ'function'なしで返すことがあるため警告は出さない。
					// fprintf(stderr, "Warning: Ignoring unexpected tool call: type='%s', name='%s' (expected '%s')\n",
					// 	call.type.c_str(), call.function.name.c_str(), tool_name.data());
				}
			}
			if (handled) return true;
		}
		return false;
	}
public:
	ChatSession() = default;
	~ChatSession()
	{
		close();
	}
	bool open()
	{
		ai_.set_ai_model(ai_model);
		if (ai_.open()) {
			return true;
		} else {
			fprintf(stderr, "Error: Failed to open AI session for model '%s' (provider: %s)\n",
				ai_model.model_name().c_str(),
				ai_model.provider_info_ ? ai_model.provider_info_->tag.c_str() : "unknown");
			return false;
		}
	}
	void close()
	{
		ai_.close();
	}
	
	bool run(Option const &opt)
	{
		while (prompt_index_ < prompts_.size()) {
			AiSession::Quert2Resuest req = prompts_[prompt_index_++];
			if (!req.internal) {
				puts("---");
				printf(">>> %s\n", req.prompt_text.c_str());
				puts("---");
				fflush(stdout);
			}

			AiResult msg = ai_.request(req);
			if (!msg) {
				fprintf(stderr, "Error: Request failed: %s\n", msg.error_message().c_str());
				continue;
			}

			if (ai_model.provider_id() == GenerativeAI::ProviderID::Anthropic) {
				if (tooluse_anthropic(msg)) continue;
			} else if (ai_model.provider_id() == GenerativeAI::ProviderID::OpenAI_responses) {
				if (tooluse_openai_responses(msg)) continue;
			} else if (ai_model.api_compatibility() == GenerativeAI::ProviderID::OpenAI_chat_completions) {
				if (tooluse_openai_chat_completion(msg)) continue;
			} else {
				fprintf(stderr, "Error: Unsupported provider tag '%s' for tool_use\n",
					ai_model.provider_info_ ? ai_model.provider_info_->tag.c_str() : "unknown");
				continue;
			}

			std::string content = msg.content();
			if (content.empty()) {
				fprintf(stderr, "Warning: Response succeeded but content is empty\n");
			}
			puts(content.c_str());
			fflush(stdout);
		}
		return true;
	}
};
static std::string default_git_command_path()
{
#if _WIN32
	return "C:\\Program Files\\Git\\cmd\\git.exe";
#else
	return "/usr/bin/git";
#endif
}

int main2(int argc, char **argv)
{
	bool f_stdin = false;

	int argi = 1;
	while (argi < argc) {
		if (*argv[argi] == '-') {
			std::string_view arg(argv[argi]);
			if (arg == "--stdin") {
				f_stdin = true;
				argi++;
			} else {
				fprintf(stderr, "Unknown option: %s\n", argv[argi]);
				return 1;
			}
		} else {
			if (!opt.prompt.empty()) {
				opt.prompt += ' ';
			}
			opt.prompt += argv[argi];
			argi++;
		}
	}

	std::string model_name = opt.model_name;
	if (model_name.empty()) {
		model_name = GenerativeAI::Model::default_model();
	}
	ai_model = GenerativeAI::Model::from_name(model_name);
	if (!ai_model) {
		fprintf(stderr, "error: Invalid model name: %s\n", model_name.c_str());
		return 1;
	}

	ChatSession session;
	if (session.open()) {
		
		session.add_chat_request("Hello");
		session.add_chat_request("Who are you?");
		session.add_chat_request("Write a haiku.");
		session.add_tool_use_request(tool.use_prompt, tool.name);
	
		session.run(opt);
	}

	return 0;
}

#ifdef _WIN32
static std::string writable_generic_config_location()
{
	PWSTR path = nullptr;

	HRESULT hr = SHGetKnownFolderPath(
		FOLDERID_LocalAppData, // %LOCALAPPDATA%
		KF_FLAG_DEFAULT,
		nullptr,
		&path);

	if (FAILED(hr)) {
		return { };
	}

	std::string result = misc::convert_wstr_to_str(path);
	CoTaskMemFree(path);

	return result;
}
#else
static std::string writable_generic_config_location()
{
	return misc::realpath("~/.config");
}
#endif

int main(int argc, char **argv)
{
	if (0) {
		return { };
	}

	std::string organization_name = "soramimi.jp";
	std::string application_name = "chat";
	// std::string this_executive_program = FileInfo(argv[0]).absoluteFilePath();
	std::string generic_config_dir = writable_generic_config_location();
	std::string app_config_dir = generic_config_dir / organization_name / application_name;
	// std::string log_dir = app_config_dir / "log";
	opt.config_file_path = app_config_dir / application_name + ".ini";
	opt.config_file_path = misc::realpath(opt.config_file_path);

	{
		ConfigParser parser;
		parser.parse(opt.config_file_path.c_str(), [](std::string const &section, std::string const &key, std::string const &value, void *cookie) {
			Option *opt = static_cast<Option *>(cookie);
			if (section == "AI") {
				if (key == "model") {
					opt->model_name = value;
				}
			} }, &opt);
	}

	return main2(argc, argv);
}
