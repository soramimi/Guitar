
// experimental code for generating commit messages using AI

#include "genmsg.h"
#include "CommitMessageGenerator.h"
#include "FileTypeDetector.h"
#include "Git.h"
#include "inetclient.h"
#include "webclient.h"
#include "Logger.h"
#include "common/misc.h"
#include "common/joinpath.h"
#include "common/q/Dir.h"

#ifdef APP_GUITAR
#include "ApplicationGlobal.h"
static GitContext gcx()
{
	return global->gcx();
}
std::string global_mimetype_by_file(std::string const &path)
{
	return global->mimetype_by_file(path);
}
#else
void global_write_log(QString const &s)
{
}

GenerativeAI::Model global_appsettings_ai_model()
{
	static GenerativeAI::Model model(GenerativeAI::AI::LLAMACPP, "llamacpp://localhost:8080/");
	return model;
}

GenerativeAI::Credential global_get_ai_credential(GenerativeAI::AI aiid)
{
	return {};
}

static WebContext webcx(WebClient::HTTP_1_0);

std::shared_ptr<AbstractInetClient> global_inet_client()
{
	std::shared_ptr<AbstractInetClient> http;
#ifdef USE_LIBCURL
	http = std::make_shared<CurlClient>(&global->curlcx);
#else
	http = std::make_shared<WebClient>(&webcx);
#endif
	return http;
}

FileTypeDetector file_type_detector;

GitContext gcx()
{
	static GitContext gcx_;

#ifdef _WIN32
	gcx_.git_command = "C:\\Program Files\\Git\\cmd\\git.exe";
#else
	gcx_.git_command = "/usr/bin/git";
#endif

	return gcx_;
}
std::string global_mimetype_by_file(std::string const &path)
{
	return file_type_detector.mimetype_by_file(path);
}
#endif


int genmsg()
{

#if 0
	std::string dir = Dir::currentPath();
#else
	std::string dir = "/home/soramimi/develop/Guitar";//Dir::currentPath();
	Dir::setCurrent(dir);
#endif
	GitRunner g(std::make_shared<Git>(gcx(), dir, std::string{}, std::string{}));


	std::vector<std::string> names = g.diff_name_only_head();

	std::vector<std::string> diffs(names.size());
	for (size_t i = 0; i < names.size(); i++) {
		std::string path = names[i];
		if (path.empty()) continue;
		std::string working_dir_path(g.workingDir() / path);
		std::string mimetype = global_mimetype_by_file(working_dir_path);
		std::string name = misc::filename(path);
		if (name == "libtool") continue; // libtoolはdiffしても大きい上に役に立たない
		if (CommitMessageGenerator::accept_file_diff(path, mimetype)) {
			std::string diff = g.diff_full_index_head_file(working_dir_path);
			logprintf(LOG_DEFAULT, "diff %s (mimetype: %s) size: %d\n", path.c_str(), mimetype.c_str(), (int)diff.size());
			if (diff.empty()) continue;
			if (diff.size() > 100000) { // 巨大なdiffはAIへの入力として不適切なので無視する
				logprintf(LOG_DEFAULT, "warning: diff for %s is too large, skipping\n", path.c_str());
				continue;
			}
			diffs[i] = diff;
		}
	}

	std::string diff;

	// スレッドごとの結果を元のファイル順に連結する
	for (size_t i = 0; i < diffs.size(); i++) {
		if (diffs[i].empty()) continue;
		diff += diffs[i];
	}

	CommitMessageGenerator gen;
	CommitMessageGenerator::Result msg = gen.generate(diff);

	for (std::string const &line : msg.messages) {
		printf("--- %s\n", line.c_str());
	}


	return 0;
}



#ifndef APP_GUITAR
int main()
{
	WebClient::initialize();
	Logger::start();

	genmsg();

	WebClient::cleanup();
	Logger::stop();
}
#endif

