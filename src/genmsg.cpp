#include "genmsg.h"
#include "ApplicationGlobal.h"
#include "ApplicationSettings.h"
#include "CommitMessageGenerator.h"
#include "Git.h"
#include "common/q/Dir.h"

static std::string mimetype_by_file(std::string const &path)
{
	return global->file_type_detector.mimetype_by_file(path);
}

int genmsg()
{
	std::string dir = Dir::currentPath();
	std::shared_ptr<Git> g = std::make_shared<Git>(global->gcx(), dir, std::string{}, std::string{});
	std::string cmd = "diff --name-only HEAD";
	auto result = g->git(cmd);
	std::vector<std::string> files = misc::splitLines(g->resultStdString(result), false);

	std::string diff;
	for (std::string const &name : files) {
		if (name.empty()) return false;
		std::string mimetype = mimetype_by_file(name);
		if (CommitMessageGenerator::accept_file_diff(name, mimetype)) {
			cmd = "diff --full-index HEAD -- " + name;
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




