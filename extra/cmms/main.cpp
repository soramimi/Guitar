
// cmms: Commit Message
//
// experimental code for generating commit messages using AI

#include "CommitMessageGenerator.h"
#include "../common/ConfigParser.h"
#include "../common/selectitem.h"
#include "FileTypeDetector.h"
#include "common/fmt.h"
#include "common/joinpath.h"
#include "common/q/FileInfo.h"
#include "common/str.h"
#include "curlclient.h"
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <string>
#include "common/wstring.h"
#include "process/ProcessWin.h"
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
	std::string model_name;
	std::string dir;
	std::string config_file_path;
	std::string git_command;
	bool git_add_A = false;
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
			fprintf(stderr, "Warning: Environment variable %s is not set. API key for %s will be empty.\n", env_name.c_str(), model.model_name().c_str());
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
	operator bool () const
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

GitReturn git(std::string const &cmd)
{
	GitReturn ret;
#if _WIN32
	ProcessWin proc;
#else
	ProcessPosix proc;
#endif
	char const *cd = ".";
	if (!opt.dir.empty()) {
		cd = opt.dir.c_str();
	}
	proc.start(fmt("%s -C %s %s")(quoted_text(opt.git_command))(quoted_text(cd))(cmd), false);
	ret.exit_code = proc.wait();
	ret.out_text = (misc::str)proc.stdout_bytes();
	return ret;
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

/**
 * @brief git status -sの結果を行ごとに分割して返す
 *
 * git status -sの出力を行ごとに分割し、std::vector<std::string>として返します。
 * 各行は、変更されたファイルの状態を表す文字列になります。
 *
 * @return git status -sの出力を行ごとに分割したリスト
 */
std::vector<std::string> git_status_s()
{
	std::vector<std::string> ret;
	std::string s = git("status -s -u --porcelain");
	std::vector<std::string_view> lines = split_lines(s);
	for (auto line: lines) {
		if (!line.empty()) {
			ret.emplace_back(line);
		}
	}
	return ret;
}

/**
 * @brief ステージされた変更が存在するかどうかを判定する
 *
 * git status -sの出力を解析して、ステージされた変更が存在するかどうかを判定します。
 * ステージされた変更が存在する場合はtrueを返し、そうでない場合はfalseを返します。
 *
 * @return ステージされた変更が存在する場合はtrue、そうでない場合はfalse
 */
bool has_staged()
{
	bool staged = false;
	std::vector<std::string> list = git_status_s();
	for (std::string const &line : list) {
		if (line.size() < 2) continue;
		char x = line[0]; // staged
		char y = line[1]; // unstaged
		if (x == '?') continue;
		if (x != ' ') {
			staged = true;
		}
	}
	return staged;
}

bool git_add_A()
{
	return git("add -A");
}

bool git_commit(std::string const &message)
{
	return git(fmt("commit -m %s")(quoted_text(message)));
}

std::vector<std::string> request(Option const &opt)
{
	std::string diff = CommitMessageGenerator::make_diff(opt.git_command, opt.dir, {});

	CommitMessageGenerator::Request request(diff, {});

	CommitMessageGenerator gen(ai_model, request);
	CommitMessageGenerator::CommitMessageGenerator::Result msg;
	if (request.diff.empty()) {
		msg.error = true;
		msg.error_message = "diff is empty";
	} else if (request.diff.size() > CommitMessageGenerator::max_diff_size) {
		// 巨大なdiffはトークン超過やコスト増加を招くため上限を設ける
		msg.error = true;
		msg.error_message = fmt("diff is too large (%d bytes)")(request.diff.size());
	} else {
		auto r = gen.generate();
		msg = CommitMessageGenerator::parse_response(ai_model, r);
	}
	if (msg.error) {
		fprintf(stderr, "Error generating commit message: %s - %s\n", msg.error_status.c_str(), msg.error_message.c_str());
		return {};
	}

	return msg.messages;
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
	int argi = 1;
	while (argi < argc) {
		if (*argv[argi] == '-') {
			std::string_view arg = argv[argi++];
			if (arg == "-A") {
				opt.git_add_A = true;
				continue;
			}
			if (arg == "-C") {
				if (argi < argc) {
					opt.dir = argv[argi];
					argi++;
				} else {
					fprintf(stderr, "error: -C option requires a directory argument\n");
					return 1;
				}
				continue;
			}
		} else {
			argi++;
		}
	}

	if (opt.git_command.empty()) {
		fmt(R"---(error: git command is not specified. Please set the git command in the configuration file.
example of : %s
---
[Programs]
git = %s
---
)---")
			(opt.config_file_path)
			(default_git_command_path())
			.err();
		return 1;
	} else {
		FileInfo info(opt.git_command);
		if (!info.isExecutable()) {
			fprintf(stderr, "error: git command not found or not executable: %s\n", opt.git_command.c_str());
			return 1;
		}
	}

	if (opt.dir.empty()) {
		// opt.dir = Dir::currentPath();
		auto ret = git("rev-parse --show-toplevel");
		if (ret) {
			opt.dir = misc::trimmed(ret.out_text);
		}
		if (opt.dir.empty()) {
			fprintf(stderr, "error: Not a git repository (or any of the parent directories). Please run this program inside a git repository.\n");
			return 1;
		}
	}

	if (opt.git_add_A) {
		git_add_A();
	}

	if (!has_staged()) {
		fprintf(stderr, "error: No staged changes found. Please stage some changes before running this program.\n");
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

	auto list = request(opt);
	if (!list.empty()) {
		int index = selectitem(list);
		if (index >= 0 && index < list.size()) {
			std::string message = list[index];
			git_commit(message);
		}
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
		&path
		);

	if (FAILED(hr)) {
		return {};
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
		std::string s = git("--version");
		puts(s.c_str());
		return {};
	}

	std::string organization_name = "soramimi.jp";
	std::string application_name = "cmms";
	std::string this_executive_program = FileInfo(argv[0]).absoluteFilePath();
	std::string generic_config_dir = writable_generic_config_location();
	std::string app_config_dir = generic_config_dir / organization_name / application_name;
	std::string log_dir = app_config_dir / "log";
	opt.config_file_path = app_config_dir / application_name + ".ini";
	opt.config_file_path = misc::realpath(opt.config_file_path);

	{
		ConfigParser parser;
		parser.parse(opt.config_file_path.c_str(), [](std::string const &section, std::string const &key, std::string const &value, void *cookie){
			Option *opt = static_cast<Option *>(cookie);
			if (section == "AI") {
				if (key == "model") {
					opt->model_name = value;
				}
			} else if (section == "Programs") {
				if (key == "git") {
					std::string s = value;
#ifdef _WIN32
					for (char &c : s) {
						if (c == '/') {
							c = '\\';
						}
					}
#endif
					opt->git_command = s;
				}
			}
		}, &opt);
	}

	return main2(argc, argv);
}
