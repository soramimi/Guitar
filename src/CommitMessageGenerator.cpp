#include "CommitMessageGenerator.h"
#include "ApplicationGlobal.h"
#include "common/jstream.h"
#include "common/strformat.h"
#include "webclient.h"
#include "GitRunner.h"
#include <QFile>

struct CommitMessageResult {
	bool completion = false;
	std::string text;
	std::string error_status;
	std::string error_message;
};

struct _CommitMessageResponseParser : public GenerativeAI::AbstractVisitor<CommitMessageResult> {
	jstream::Reader reader;
	_CommitMessageResponseParser(std::string_view const &in)
		: reader(in.data(), in.data() + in.size())
	{}

	CommitMessageResult parse_openai_format()
	{
		CommitMessageResult ret;
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

	CommitMessageResult case_Unknown()
	{
		return {};
	}

	CommitMessageResult case_OpenAI()
	{
		return parse_openai_format();
	}

	CommitMessageResult case_Anthropic()
	{
		CommitMessageResult ret;
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

	CommitMessageResult case_DeepSeek()
	{
		return parse_openai_format();
	}

	CommitMessageResult case_OpenRouter()
	{
		return parse_openai_format();
	}

	CommitMessageResult case_Ollama()
	{
		CommitMessageResult ret;
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

	CommitMessageResult case_LMStudio()
	{
		return parse_openai_format();
	}
};

/**
 * @brief Generate a JSON string for the given AI model.
 * @param model The AI model.
 * @param diff The diff to generate the commit message for.
 * @param max_message_count The maximum number of messages to generate.
 * @return The JSON string.
 */
struct _PromptJsonGenerator : public GenerativeAI::AbstractVisitor<std::string> {
	std::string modelname;
	std::string prompt;
	_PromptJsonGenerator(std::string const &modelname, std::string const &prompt)
		: modelname(modelname)
		, prompt(prompt)
	{}

	std::string case_Unknown()
	{
		return {};
	}

	std::string case_OpenAI()
	{
		std::string json = R"---({
"model": "%s",
"messages": [
	{"role": "system", "content": "You are an experienced engineer."},
	{"role": "user", "content": "%s"}]
})---";
		return strf(json)(modelname)(jstream::encode_json_string(prompt));
	}

	std::string case_Anthropic()
	{
		std::string json = R"---({
"model": "%s",
"messages": [
	{"role": "user", "content": "%s"}
],
"max_tokens": %d,
"temperature": 0.7
})---";
		return strf(json)(modelname)(jstream::encode_json_string(prompt))(200);
	}

	std::string case_Google()
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

	std::string case_DeepSeek()
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

	std::string case_Ollama()
	{
		std::string json = R"---({
"model": "%s",
"prompt": "%s",
"stream": false
})---";
		return strf(json)(jstream::encode_json_string(modelname))(jstream::encode_json_string(prompt));
	}

	std::string case_OpenRouter()
	{
		return case_OpenAI();
	}

	std::string case_LMStudio()
	{
		std::string json = R"---({
"model": "%s",
"prompt": "%s",
"stream": false
})---";
		return strf(json)(jstream::encode_json_string(modelname))(jstream::encode_json_string(prompt));
	}
};

/**
 * @brief Parse the response from the AI model.
 * @param in The response.
 * @param ai_type The AI model type.
 * @return The generated commit message.
 */
CommitMessageGenerator::Result CommitMessageGenerator::parse_response(std::string const &in, GenerativeAI::AI provider)
{
	CommitMessageResult r = _CommitMessageResponseParser(in).visit(provider);

	if (r.completion) {
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

std::string CommitMessageGenerator::generate_prompt_json(GenerativeAI::Model const &model, std::string const &prompt)
{
	return _PromptJsonGenerator(model.model_name(), prompt).visit(model.provider_id());
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
	
	if (diff.empty()) {
		return Error("error", "diff is empty");
	}

	if (diff.size() > 100000) {
		return Error("error", "diff too large");
	}

	GenerativeAI::Model model = global->appsettings.ai_model;
	if (model.model_name().empty()) {
		return Error("error", "AI model is not set.");
	}
	
	std::string prompt = generatePrompt(diff, max_message_count);
	std::string json = generate_prompt_json(model, prompt);
	
	if (save_log) {
		QFile file("c:\\a\\request.json");
		if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			file.write(json.c_str(), json.size());
			file.close();
		} else {
			qDebug() << "Failed to write request JSON to file:" << file.errorString();
		}
	}

	GenerativeAI::Credential cred = global->get_ai_credential(model.provider_id());
	GenerativeAI::Request ai_req = GenerativeAI::make_request(model.provider_id(), model, cred);

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
			QFile file("c:\\a\\response.txt");
			if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
				file.write(data, size);
				file.close();
			} else {
				qDebug() << "Failed to write response to file:" << file.errorString();
			}
		}
		std::string text(data, size);
		CommitMessageGenerator::Result ret = parse_response(text, model.provider_id());
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
