#include "genmsg.h"
#include "ApplicationGlobal.h"
#include "ApplicationSettings.h"
#include "CommitMessageGenerator.h"
#include "Git.h"
#include "common/q/Dir.h"

static std::string determineFileType(std::string const &path)
{
	return global->determineFileType(path);
}

int genmsg()
{
	std::string dir = Dir::currentPath();
	std::shared_ptr<Git> g = std::make_shared<Git>(global->gcx(), dir, std::string{}, std::string{});
	std::string cmd = "diff --name-only HEAD";
	auto result = g->git(cmd);
	std::vector<std::string> files = misc::splitLines(g->resultStdString(result), false);

	std::string diff;
	for (std::string const &file : files) {
		auto Accept = [&](){
			if (file.empty()) return false;
			std::string mimetype = determineFileType(file);
			if (misc::starts_with(mimetype, "image/")) return false; // 画像ファイルはdiffしない
			if (mimetype == "application/octet-stream") return false; // バイナリファイルはdiffしない
			if (mimetype == "application/pdf") return false; // PDFはdiffしない
			if (mimetype == "text/xml" && misc::ends_with(file, ".ts")) return false; // Do not diff Qt translation TS files (line numbers and other changes are too numerous)
			return true;
		};
		if (Accept()) {
			cmd = "diff --full-index HEAD -- " + file;
			auto result = g->git(cmd);
			diff.append(g->resultStdString(result));
		}
	}

	CommitMessageGenerator gen;
	CommitMessageGenerator::Result msg = gen.generate(diff);

	for (std::string const &line : msg.messages) {
		printf("--- %s\n", line.c_str());
	}

	return 0;
}




