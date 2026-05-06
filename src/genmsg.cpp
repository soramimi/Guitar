
// experimental code for generating commit messages using AI

#include "CommitMessageGenerator.h"
#include "FileTypeDetector.h"
#include "common/fmt.h"
#include "common/q/Dir.h"
#include "common/str.h"
#include "curlclient.h"
#include <string_view>
#include <selectitem.h>

#ifdef _WIN32
#include <WinSock2.h>
#endif

#ifdef _WIN32
char const *gitcommand = "C:\\Program Files\\Git\\cmd\\git.exe";
#else
char const *gitcommand = "/usr/bin/git";
#endif

static CurlContext curlcx;
static GenerativeAI::Model ai_model;

struct Option {
	std::string dir;

};

Option opt;

// CommitMessageGeneratorからコールバックされる関数

void global_write_log(QString const &s)
{
}

GenerativeAI::Model global_appsettings_ai_model()
{
	return ai_model;
}

GenerativeAI::Credential global_get_ai_credential(GenerativeAI::AI aiid)
{
	GenerativeAI::Credential cred;
	std::string env_name = ai_model.env_name();
	if (!env_name.empty()) {
		char const *env = std::getenv(env_name.c_str());
		if (env) {
			cred.api_key = env;
		} else {
			fprintf(stderr, "Warning: Environment variable %s is not set. API key for %s will be empty.\n", env_name.c_str(), ai_model.long_name().c_str());
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

GitReturn git(std::string const &cmd)
{
	GitReturn ret;
	Process proc;
	char const *cd = ".";
	if (!opt.dir.empty()) {
		cd = opt.dir.c_str();
	}
	proc.start(fmt("\"%s\" -C \"%s\" %s")(gitcommand)(cd)(cmd), false);
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

bool commit(std::string const &message)
{
	return git(fmt("commit -m \"%s\"")(message));
}

std::vector<std::string> genmsg(Option const &opt)
{
	if (0) {
		std::string s = git("--version");
		puts(s.c_str());
		return {};
	}

	std::string diff = CommitMessageGenerator::diff_head(gitcommand, opt.dir);

	CommitMessageGenerator gen;
	CommitMessageGenerator::Result msg = gen.generate(diff);

	return msg.messages;
}

int main2(int argc, char **argv)
{
	int argi = 1;
	while (argi < argc) {
		if (*argv[argi] == '-') {
			std::string_view arg = argv[argi++];
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

	if (opt.dir.empty()) {
		opt.dir = Dir::currentPath();
	}

	if (!has_staged()) {
		fprintf(stderr, "error: No staged changes found. Please stage some changes before running this program.\n");
		return 1;
	}

	ai_model = GenerativeAI::Model::from_name("gpt-5.4-mini");

	auto list = genmsg(opt);
	if (!list.empty()) {
		int index = selectitem(list);
		if (index >= 0 && index < list.size()) {
			std::string message = list[index];
			commit(message);
		}
	}

	return 0;
}

void cleanup()
{
#if USE_OPENSSL
	ERR_free_strings();
#endif
#ifdef _WIN32
	WSACleanup();
#endif
}

void startup()
{
#ifdef _WIN32
	{
		WSADATA wsaData;
		WORD wVersionRequested;
		wVersionRequested = MAKEWORD(2, 2); // Request version 2.2 for better compatibility
		if (WSAStartup(wVersionRequested, &wsaData) == 0) {
			atexit(cleanup);
		}
	}
#endif
}

int main(int argc, char **argv)
{
	startup();

	int ret = main2(argc, argv);

	cleanup();
	return ret;
}

