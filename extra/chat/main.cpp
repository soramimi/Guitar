
// cmms: Commit Message
//
// experimental code for generating commit messages using AI

#include "../common/ConfigParser.h"
#include "../common/selectitem.h"
#include "AiApiBridge.h"
#include "FileTypeDetector.h"
#include "common/fmt.h"
#include "common/joinpath.h"
#include "common/q/FileInfo.h"
#include "common/str.h"
#include "curlclient.h"

#ifdef _WIN32
#include "common/wstring.h"
#include "process/ProcessWin.h"
#include <shlobj.h>
#include <string>
#include <windows.h>
#else
#include "process/ProcessPosix.h"
#endif

namespace misc {

std::string realpath(const char *path);
std::string realpath(std::string const &path);

}

static CurlContext curlcx;
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

std::string request(Option const &opt)
{
	AiApiBridge gen;
	gen.set_ai_model(ai_model);
	AiResult msg = gen.query(opt.prompt);
	if (!msg) {
		fprintf(stderr, "Error generating commit message: %s - %s\n", msg.error_status().c_str(), msg.error_message().c_str());
		return { };
	}

	return msg.content();
}

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

	if (opt.prompt.empty()) {
		if (f_stdin) {
			while (1) {
				char *line = nullptr;
				size_t len = 0;
				if (getline(&line, &len, stdin) == -1) break;
				std::string_view sv(line, std::strlen(line));
				opt.prompt += sv;
				free(line);
			}
		}
	}

	if (opt.prompt.empty()) {
		fprintf(stderr, "Usage: %s [options] <prompt>\n", argv[0]);
		return 1;
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

	std::string msg = request(opt);
	if (!msg.empty()) {
		puts(msg.c_str());
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
