#include "CommitMessageGenerator.h"
#include "ApplicationGlobal.h"
#include "common/jstream.h"
#include "common/rwfile.h"
#include "common/strformat.h"
#include "webclient.h"

struct CommitMessageResponseParser {
	struct Result {
		bool completion = false;
		std::string text;
		std::string error_status;
		std::string error_message;
	};

	jstream::Reader reader;
	CommitMessageResponseParser(std::string_view const &in)
		: reader(in.data(), in.data() + in.size())
	{}

	Result parse_openai_format()
	{
		Result ret;
		while (reader.next()) {
			if (reader.match("{object")) {
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

	Result operator () (GenerativeAI::Unknown const &provider)
	{
		return {};
	}

	Result operator () (GenerativeAI::OpenAI const &provider)
	{
		return parse_openai_format();
	}

	Result operator () (GenerativeAI::Anthropic const &provider)
	{
		Result ret;
		while (reader.next()) {
			if (reader.match("{stop_reason")) {
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

	Result operator () (GenerativeAI::Google const &provider)
	{
		Result ret;
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

	Result operator () (GenerativeAI::DeepSeek const &provider)
	{
		return parse_openai_format();
	}

	Result operator () (GenerativeAI::OpenRouter const &provider)
	{
		return parse_openai_format();
	}

	Result operator () (GenerativeAI::Ollama const &provider)
	{
		Result ret;
		while (reader.next()) {
			if (reader.match("{model")) {
				reader.string();
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

	Result operator () (GenerativeAI::LMStudio const &provider)
	{
		return parse_openai_format();
	}

	static Result parse(GenerativeAI::Provider const &provider, std::string_view const &in)
	{
		return std::visit(CommitMessageResponseParser{in}, provider);
	}
};

/**
 * @brief Parse the response from the AI model.
 * @param in The response.
 * @param ai_type The AI model type.
 * @return The generated commit message.
 */
CommitMessageGenerator::Result CommitMessageGenerator::parse_response(std::string const &in, GenerativeAI::Provider const &provider)
{
	auto r = CommitMessageResponseParser::parse(provider, in);

	if (r.completion) {
		if (kind == CommitMessage) {
			std::vector<std::string_view> lines = misc::splitLinesV(r.text, false);
			size_t i = lines.size();
			while (i > 0) {
				i--;
				std::string_view sv = lines[i];
				char const *ptr = sv.data();
				char const *end = ptr + sv.size();
				while (ptr + 1 < end && *ptr == '`' && end[-1] == '`') {
					ptr++;
					end--;
				}
				bool accept = false;

				if (ptr < end && *ptr == '-') {
					accept = true;
					ptr++;
					while (ptr < end && (*ptr == '-' || isspace((unsigned char)*ptr))) { // e.g. "- - message"
						ptr++;
					}
				} else if (isdigit((unsigned char)*ptr)) {
					while (ptr < end && isdigit((unsigned char)*ptr)) {
						accept = true;
						ptr++;
					}
					if (ptr < end && *ptr == '.') {
						ptr++;
					}
				}
				if (accept) {
					while (ptr < end && isspace((unsigned char)*ptr)) {
						ptr++;
					}
					if (ptr + 1 < end && *ptr == '\"' && end[-1] == '\"') {
						ptr++;
						end--;
					}
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
		}
		return {};
	} else {
		CommitMessageGenerator::Result ret;
		ret.error = true;
		ret.error_status = r.error_status;
		ret.error_message = r.error_message;
		if (ret.error_message.empty()) {
			ret.error_message = in;
		}
		return ret;
	}
}

/**
 * @brief Generate a prompt for the given diff.
 * @param diff The diff.
 * @param max The maximum number of messages to generate.
 * @return The prompt.
 */
std::string CommitMessageGenerator::generatePrompt(std::string const &diff, int max)
{
	std::string prompt = strf(
		"Generate a concise git commit message written in present tense for the following code diff with the given specifications below. "
		"Please generate %d messages, bulleted, and start writing with '-'. "
		"No headers and footers other than bulleted messages. "
		)(max);
	prompt = prompt + "\n\n" + diff;
	return prompt;
}

/**
 * @brief Generate a JSON string for the given AI model.
 * @param model The AI model.
 * @param diff The diff to generate the commit message for.
 * @param max_message_count The maximum number of messages to generate.
 * @return The JSON string.
 */
std::string CommitMessageGenerator::generatePromptJSON(std::string const &prompt, GenerativeAI::Model const &model)
{
	struct PromptJsonGenerator {
		Kind kind;
		std::string prompt;
		std::string modelname;
		PromptJsonGenerator(std::string const &prompt, std::string const &modelname, Kind kind)
			: kind(kind), prompt(prompt)
			, modelname(modelname)
		{}

		std::string generate_openai_format(std::string modelname)
		{
			std::string json = R"---({
	"model": "%s",
	"messages": [
		{"role": "system", "content": "You are an experienced engineer."},
		{"role": "user", "content": "%s"}]
})---";
			return strf(json)(modelname)(jstream::encode_json_string(prompt));
		}

		std::string operator () (GenerativeAI::Unknown const &provider)
		{
			return {};
		}

		std::string operator () (GenerativeAI::OpenAI const &provider)
		{
			return generate_openai_format(modelname);
		}

		std::string operator () (GenerativeAI::Anthropic const &provider)
		{
			std::string json = R"---({
	"model": "%s",
	"messages": [
		{"role": "user", "content": "%s"}
	],
	"max_tokens": %d,
	"temperature": 0.7
})---";
			return strf(json)(modelname)(jstream::encode_json_string(prompt))(kind == CommitMessage ? 200 : 1000);
		}

		std::string operator () (GenerativeAI::Google const &provider)
		{
			std::string json = R"---({
	"contents": [{
		"parts": [{
			"text": "%s"
		}]
	}]
})---";
			return strf(json)(jstream::encode_json_string(prompt));
		}

		std::string operator () (GenerativeAI::DeepSeek const &provider)
		{
			std::string json = R"---({
	"model": "%s",
	"messages": [
		{"role": "system", "content": "You are an experienced engineer."},
		{"role": "user", "content": "%s"}
	],
	"stream": false
})---";
			return strf(json)(modelname)(jstream::encode_json_string(prompt));
		}

		std::string operator () (GenerativeAI::Ollama const &provider)
		{
			std::string json = R"---({
	"model": "%s",
	"prompt": "%s",
	"stream": false
})---";
			return strf(json)(jstream::encode_json_string(modelname))(jstream::encode_json_string(prompt));
		}

		std::string operator () (GenerativeAI::OpenRouter const &provider)
		{
			return generate_openai_format(modelname);
		}

		std::string operator () (GenerativeAI::LMStudio const &provider)
		{
			std::string json = R"---({
	"model": "%s",
	"prompt": "%s",
	"stream": false
})---";
			return strf(json)(jstream::encode_json_string(modelname))(jstream::encode_json_string(prompt));
		}

		static std::string generate(std::string const &prompt, GenerativeAI::Provider const &provider, GenerativeAI::Model const &model, Kind kind)
		{
			return std::visit(PromptJsonGenerator{prompt, model.model_name(), kind}, provider);
		}
	};

	return PromptJsonGenerator::generate(prompt, model.provider, model, kind);
}



/**
 * @brief Generate a commit message using the given diff.
 * @param g The Git object.
 * @return The generated commit message.
 */
CommitMessageGenerator::Result CommitMessageGenerator::generate(std::string const &diff, QString const &hint)
{
	constexpr int max_message_count = 5;
	
	constexpr bool save_log = false;
	
	if (diff.empty()) return {};

	if (diff.size() > 100000) {
		return Error("error", "diff too large");
	}

	GenerativeAI::Model model = global->appsettings.ai_model;
	if (model.model_name().empty()) {
		return Error("error", "AI model is not set.");
	}
	
	std::string prompt;
	switch (kind) {
	case CommitMessage:
		prompt = generatePrompt(diff, max_message_count);
		break;
	default:
		return {};
	}
	
	std::string json = generatePromptJSON(prompt, model);
	
	if (save_log) {
		writefile("c:\\a\\request.txt", json.c_str(), json.size());
	}

	GenerativeAI::Credential cred = global->get_ai_credential(model.provider);
	GenerativeAI::Request ai_req = GenerativeAI::make_request(model.provider, model, cred);

	WebClient::Request web_req;
	web_req.set_location(ai_req.endpoint_url);
	for (std::string const &h : ai_req.header) {
		web_req.add_header(h);
	}

	WebClient::Post post;
	post.content_type = "application/json";
	post.data.insert(post.data.end(), json.begin(), json.end());

	WebClient http(&global->webcx);
	if (http.post(web_req, &post)) {
		char const *data = http.content_data();
		size_t size = http.content_length();
		if (save_log) {
			writefile("c:\\a\\response.txt", data, size);
		}
		std::string text(data, size);
		CommitMessageGenerator::Result ret = parse_response(text, model.provider);
		return ret;
	}

	return {};
}

std::string CommitMessageGenerator::diff_head(GitRunner g)
{
	std::string diff = g.diff_head([&](std::string const &name, std::string const &mime) {
		if (mime == "text/xml" && misc::ends_with(name, ".ts")) return false; // Do not diff Qt translation TS files (line numbers and other changes are too numerous)
		return true;
	});
	return diff;
}





