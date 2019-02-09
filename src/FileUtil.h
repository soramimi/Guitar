#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <string>
#include <vector>
#include <QString>

class FileUtil {
public:
	struct DirEnt {
		std::string name;
		bool isdir = false;
	};

	static std::string getcwd();
	static void mkdir(std::string const &dir, int perm = 0777);
	static void rmdir(std::string const &dir);
	static bool chdir(std::string const &dir);
	static void rmfile(std::string const &path);
	static void rmtree(std::string const &path);
	static void mv(std::string const &src, std::string const &dst);
	static void getdirents(std::string const &loc, std::vector<DirEnt> *out);
	static bool isdir(std::string const &path);
	static std::string which(std::string const &name, std::vector<std::string> *out = nullptr);
	static std::string normalize_path_separator(std::string const &s);
	static void qgetdirents(const QString &loc, std::vector<DirEnt> *out);
	static std::string qwhich(const std::string &name, std::vector<std::string> *out);
};

#endif // FILEUTIL_H
