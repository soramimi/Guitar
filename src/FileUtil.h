#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <string>
#include <vector>

class FileUtil {
public:
	struct DirEnt {
		std::string name;
		bool isdir = false;
	};

	static std::string getcwd();
	static void mkdir(std::string const &dir, int perm = 0777);
	static void rmdir(const std::string &dir);
	static bool chdir(std::string const &dir);
	static void rmfile(const std::string &path);
	static void rmtree(const std::string &path);
	static void mv(const std::string &src, const std::string &dst);
	static void getdirents(std::string const &loc, std::vector<DirEnt> *out);
	static bool isdir(const std::string &path);
	static std::string which(const std::string &name, std::vector<std::string> *out = nullptr);
	static std::string normalize_path_separator(const std::string &s);
};

#endif // FILEUTIL_H
