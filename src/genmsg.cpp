
// experimental code for generating commit messages using AI

#include "CommitMessageGenerator.h"
#include "FileTypeDetector.h"
#include "common/fmt.h"
#include "common/q/Dir.h"
#include "common/str.h"
#include "curlclient.h"

static CurlContext curlcx;
static GenerativeAI::Model ai_model;

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
	char const *env = std::getenv("OPENAI_API_KEY");
	GenerativeAI::Credential cred;
	cred.api_key = env ? env : "";
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

std::string git(std::string const &cmd)
{
	Process proc;
	proc.start(fmt("\"/usr/bin/git\" %s")(cmd), false);
	proc.wait();
	std::string s = (misc::str)proc.stdout_bytes();
	return s;
}

struct Option {
	std::string dir;

};

int genmsg(Option const &opt)
{
	if (0) {
		std::string s = git("--version");
		puts(s.c_str());
		return 0;
	}

	std::string gitcommand = "/usr/bin/git";
	std::string diff = CommitMessageGenerator::diff_head(gitcommand, opt.dir);

	CommitMessageGenerator gen;
	CommitMessageGenerator::Result msg = gen.generate(diff);

	for (std::string const &line : msg.messages) {
		printf("- %s\n", line.c_str());
	}

	return 0;
}

int main(int argc, char **argv)
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

	Option opt;

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

	ai_model = GenerativeAI::Model::from_name("gpt-5.4-mini");

	genmsg(opt);

#ifdef _WIN32
	WSACleanup();
#endif
}

