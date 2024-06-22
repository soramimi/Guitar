#include "CommitMessageGenerator.h"
#include "ApplicationGlobal.h"
#include "ApplicationSettings.h"
#include "common/jstream.h"
#include "common/misc.h"
#include "common/strformat.h"
#include "webclient.h"
#include <QFile>
#include <QMessageBox>
#include <QString>

namespace {

/**
 * @brief Encode a string for JSON.
 * @param in The string to encode.
 * @return The encoded string.
 */
std::string encode_json_string(std::string const &in)
{
	std::string out;
	char const *ptr = in.c_str();
	char const *end = ptr + in.size();
	while (ptr < end) {
		char c = *ptr++;
		if (c == '"') {
			out += "\\\"";
		} else if (c == '\\') {
			out += "\\\\";
		} else if (c == '\b') {
			out += "\\b";
		} else if (c == '\f') {
			out += "\\f";
		} else if (c == '\n') {
			out += "\\n";
		} else if (c == '\r') {
			out += "\\r";
		} else if (c == '\t') {
			out += "\\t";
		} else if (c < 32) {
			char tmp[10];
			sprintf(tmp, "\\u%04x", c);
			out += tmp;
		} else {
			out += c;
		}
	}
	return out;
}

/**
 * @brief Decode a JSON string.
 * @param in The JSON string.
 * @return The decoded string.
 */
std::string decode_json_string(std::string const &in)
{
	QString out;
	char const *ptr = in.c_str();
	char const *end = ptr + in.size();
	while (ptr < end) {
		char c = *ptr++;
		if (c == '\\') {
			if (ptr < end) {
				char d = *ptr++;
				if (d == '"') {
					out += '"';
				} else if (d == '\\') {
					out += '\\';
				} else if (d == '/') {
					out += '/';
				} else if (d == 'b') {
					out += '\b';
				} else if (d == 'f') {
					out += '\f';
				} else if (d == 'n') {
					out += '\n';
				} else if (d == 'r') {
					out += '\r';
				} else if (d == 't') {
					out += '\t';
				} else if (d == 'u') {
					if (ptr + 4 <= end) {
						char tmp[5];
						memcpy(tmp, ptr, 4);
						tmp[4] = 0;
						ushort c = strtol(tmp, nullptr, 16);
						out += QChar(c);
						ptr += 4;
					}
				}
			}
		} else {
			out += c;
		}
	}
	return out.toStdString();
}

} // namespace

static std::string example_gpt_response()
{
	return R"---({
	  "id": "chatcmpl-9Q9tzFdQIw3NYSpwbgyFrG8EOJw29",
	  "object": "chat.completion",
	  "created": 1716021619,
	  "model": "gpt-4-0613",
	  "choices": [
		{
		  "index": 0,
		  "message": {
			"role": "assistant",
			"content": "- \"Upgrade C++ version from C++11 to C++17 in strformat.pro\"\n- \"Update strformat.pro to use C++17 instead of C++11\"\n- \"Change C++ version in CONFIG from C++11 to C++17 in strformat.pro\""
		  },
		  "logprobs": null,
		  "finish_reason": "stop"
		}
	  ],
	  "usage": {
		"prompt_tokens": 145,
		"completion_tokens": 60,
		"total_tokens": 205
	  },
	  "system_fingerprint": null
	}
)---";
}

static std::string example_claude_response()
{
	return R"---({
	"id":"msg_01HqUHZ5u6uVJnZBANdU3iRx",
	"type":"message",
	"role":"assistant",
	"model":"claude-3-opus-20240229",
	"content":[
		{
			"type":"text",
			"text":"- Add support for Anthropic Claude API for generating commit messages\n- Switch to Claude-3-Opus model for commit message generation\n- Update JSON payload and headers for Anthropic API compatibility\n- Increase max_tokens to 200 and set temperature to 0.7 for generation\n- Enable writing API response to file for debugging purposes"
		}
	],
	"stop_reason":"end_turn",
	"stop_sequence":null,
	"usage":{
		"input_tokens":1066,
		"output_tokens":77
	}
}
)---";
}

static std::string example_gemini_response()
{
	return R"---({
  "candidates": [
    {
      "content": {
        "parts": [
          {
            "text": "- Adds support for the Gemini Pro model.\n- Implements Gemini support for commit message generation.\n- Adds Gemini API key to application settings.\n- Integrates Gemini Pro model into the commit message generator.\n- Enables generating commit messages using Google's Gemini Pro model. \n"
          }
        ],
        "role": "model"
      },
      "finishReason": "STOP",
      "index": 0,
      "safetyRatings": [
        {
          "category": "HARM_CATEGORY_SEXUALLY_EXPLICIT",
          "probability": "NEGLIGIBLE"
        },
        {
          "category": "HARM_CATEGORY_HATE_SPEECH",
          "probability": "NEGLIGIBLE"
        },
        {
          "category": "HARM_CATEGORY_HARASSMENT",
          "probability": "NEGLIGIBLE"
        },
        {
          "category": "HARM_CATEGORY_DANGEROUS_CONTENT",
          "probability": "NEGLIGIBLE"
        }
      ]
    }
  ],
  "usageMetadata": {
    "promptTokenCount": 1731,
    "candidatesTokenCount": 56,
    "totalTokenCount": 1787
  }
}
)---";
}

/**
 * @brief Parse the response from the AI model.
 * @param in The response.
 * @param ai_type The AI model type.
 * @return The generated commit message.
 */
GeneratedCommitMessage CommitMessageGenerator::parse_response(std::string const &in, GenerativeAI::Type ai_type)
{
	error_message_.clear();
	bool ok1 = false;
	std::string text;
	char const *begin = in.c_str();
	char const *end = begin + in.size();
	jstream::Reader r(begin, end);
	if (ai_type == GenerativeAI::GPT) {
		while (r.next()) {
			if (r.match("{object")) {
				if (r.string() == "chat.completion") {
					ok1 = true;
				}
			} else if (r.match("{choices[{message{content")) {
				text = decode_json_string(r.string());
			} else if (r.match("{error{type")) {
				error_status_ = r.string();
				ok1 = false;
			} else if (r.match("{error{message")) {
				error_message_ = r.string();
				ok1 = false;
			}
		}
	} else if (ai_type == GenerativeAI::CLAUDE) {
		while (r.next()) {
			if (r.match("{stop_reason")) {
				if (r.string() == "end_turn") {
					ok1 = true;
				} else {
					ok1 = false;
					error_status_ = r.string();
				}
			} else if (r.match("{content[{text")) {
				text = decode_json_string(r.string());
			} else if (r.match("{type")) {
				if (r.string() == "error") {
					ok1 = false;
				}
			} else if (r.match("{error{type")) {
				error_status_ = r.string();
				ok1 = false;
			} else if (r.match("{error{message")) {				
				error_message_ = r.string();
				ok1 = false;
			}
		}
	} else if (ai_type == GenerativeAI::GEMINI) {
		while (r.next()) {
			if (r.match("{candidates[{content{parts[{text")) {
				text = decode_json_string(r.string());
				ok1 = true;
			} else if (r.match("{error{message")) {
				error_message_ = r.string();
				ok1 = false;
			} else if (r.match("{error{status")) {
				error_status_ = r.string();
				ok1 = false;
			}
		}
	}
	if (ok1) {
		if (kind == CommitMessage) {
			std::vector<std::string> lines;
			misc::splitLines(text, &lines, false);
			size_t i = lines.size();
			while (i > 0) {
				i--;
				std::string_view sv = lines[i];
				char const *ptr = sv.data();
				char const *end = ptr + sv.size();
				while (ptr < end && *ptr == '`') ptr++;
				while (ptr < end && end[-1] == '`') end--;
				bool accept = false;
				if (ptr < end && *ptr == '-') {
					accept = true;
					ptr++;
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
					lines[i] = std::string(ptr, end);
				} else {
					lines.erase(lines.begin() + i);
				}
			}
			QStringList ret;
			for (auto const &line : lines) {
				ret.push_back(QString::fromStdString(line));
			}
			return ret;
		} else if (kind == DetailedComment) {
			QStringList ret;
			char const *begin = text.c_str();
			char const *end = begin + text.size();
			char const *ptr = begin;
			while (ptr < end) {
				while (ptr < end && isspace((unsigned char)*ptr)) ptr++;
				char const *left = ptr;
				if (ptr < end && isdigit((unsigned char)*ptr)) {
					do {
						ptr++;
					} while (ptr < end && isdigit((unsigned char)*ptr));
					if (ptr < end && *ptr == '.') ptr++;
				}
				char quote = 0;
				while (1) {
					int c = 0;
					if (ptr < end) {
						c = *ptr;
					}
					if (quote) {
						if (*ptr == quote) {
							quote = 0;
						}
					} else {
						if (*ptr == '"' || *ptr == '\'' || *ptr == '`') {
							quote = *ptr;
						} else if (*ptr == '.' || *ptr == '\n' || c == 0) {
							if (*ptr == '.') ptr++;
							if (ptr > left) {
								ret.push_back(QString::fromStdString(std::string(left, ptr)));
							}
							break;
						}
					}
					ptr++;
				}
			}
			return ret;
		} else {
			return {}; // invalid kind
		}
	} else {
		GeneratedCommitMessage ret;
		ret.error = true;
		ret.error_status = QString::fromStdString(error_status_);
		ret.error_message = QString::fromStdString(error_message_);
		return ret;
	}
}

/**
 * @brief Generate a prompt for the given diff.
 * @param diff The diff.
 * @param max The maximum number of messages to generate.
 * @return The prompt.
 */
std::string CommitMessageGenerator::generatePrompt(QString const &diff, int max)
{
	std::string prompt = strformat(
		"Generate a concise git commit message written in present tense for the following code diff with the given specifications below. "
		"Please generate %d messages, bulleted, and start writing with '-'. "
		"No headers and footers other than bulleted messages. "
		)(max);
	prompt = prompt + "\n\n" + diff.toStdString();
	return prompt;
}

std::string CommitMessageGenerator::generateDetailedPrompt(QString const &diff, const QString &commit_message)
{
	std::string prompt = strformat(
		"Generate a supplemental comment explaining a bit more about the following commit message to `git diff` below. "
		"Please keep it concise, about 5 lines or less of comment. "
		"Output should be body text only; do not generate headers, footers, and greetings. "
		"Omit anything like \"The commit\", \"This commit\" or \"This changes\" at the beginning."
		"The commit message: %s \n"
		"The git diff:\n"
		)(commit_message.toStdString());
	prompt = prompt + "\n\n" + diff.toStdString();
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
	std::string json;
	
	auto type = model.type();
	if (type == GenerativeAI::GPT) {
		
		json = R"---({
	"model": "%s",
	"messages": [
		{"role": "system", "content": "You are a experienced engineer."},
		{"role": "user", "content": "%s"}]
})---";
		json = strformat(json)(model.name.toStdString())(encode_json_string(prompt));
		
	} else if (type == GenerativeAI::CLAUDE) {
		
		json = R"---({
	"model": "%s",
	"messages": [
		{"role": "user", "content": "%s"}
	]
	,
	"max_tokens": %d,
	"temperature": 0.7
})---";
		json = strformat(json)(model.name.toStdString())(encode_json_string(prompt))(kind == CommitMessage ? 200 : 1000);
		
	} else if (type == GenerativeAI::GEMINI) {
		
		json = R"---({
	"contents": [{
		"parts": [{
			"text": "%s"
		}]
	}]
})---";
		json = strformat(json)(encode_json_string(prompt));
		
	} else {
		return {};
	}

	return json;
}

GeneratedCommitMessage CommitMessageGenerator::test()
{
	std::string s = R"---(
)---";
	return parse_response(s, GenerativeAI::CLAUDE);
}

/**
 * @brief Generate a commit message using the given diff.
 * @param g The Git object.
 * @return The generated commit message.
 */
GeneratedCommitMessage CommitMessageGenerator::generate(QString const &diff, QString const &hint)
{
	constexpr int max_message_count = 5;
	
	constexpr bool save_log = false;
	
	if (0) { // for debugging JSON parsing
		return test();
	}
	
	if (diff.isEmpty()) return {};

	if (diff.size() > 100000) {
		return GeneratedCommitMessage::Error("error", "diff too large");
	}

	GenerativeAI::Model model = global->appsettings.ai_model;
	if (model.name.isEmpty()) {
		return GeneratedCommitMessage::Error("error", "AI model is not set.");
	}
	
	std::string url;
	std::string apikey;
	WebClient::Request rq;
	
	auto model_type = model.type();
	if (model_type == GenerativeAI::GPT) {
		url = "https://api.openai.com/v1/chat/completions";
		apikey = global->OpenAiApiKey().toStdString();
		rq.add_header("Authorization: Bearer " + apikey);
	} else if (model_type == GenerativeAI::CLAUDE) {
		url = "https://api.anthropic.com/v1/messages";
		apikey = global->AnthropicAiApiKey().toStdString();
		rq.add_header("x-api-key: " + apikey);
		rq.add_header("anthropic-version: " + model.anthropic_version().toStdString());
	} else if (model_type == GenerativeAI::GEMINI) {
		apikey = global->GoogleApiKey().toStdString();
		url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-pro-latest:generateContent?key=" + apikey;
	} else {
		return {};
	}
	rq.set_location(url);
	
	
	std::string prompt;
	switch (kind) {
	case CommitMessage:
		prompt = generatePrompt(diff, max_message_count);
		break;
	case DetailedComment:
		prompt = generateDetailedPrompt(diff, hint);
		break;
	default:
		return {};
	}
	
	
	std::string json = generatePromptJSON(prompt, model);
	
	if (save_log) {
		QFile file("/tmp/request.txt");
		if (file.open(QIODevice::WriteOnly)) {
			file.write(json.c_str(), json.size());
		}
	}

	WebClient::Post post;
	post.content_type = "application/json";
	post.data.insert(post.data.end(), json.begin(), json.end());

	WebClient http(&global->webcx);
	if (http.post(rq, &post)) {
		char const *data = http.content_data();
		size_t size = http.content_length();
		if (save_log) {
			QFile file("/tmp/response.txt");
			if (file.open(QIODevice::WriteOnly)) {
				file.write(data, size);
			}
		}
		std::string text(data, size);
		GeneratedCommitMessage ret = parse_response(text, model_type);
		return ret;
	}

	return {};
}

