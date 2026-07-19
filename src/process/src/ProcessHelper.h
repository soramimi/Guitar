#ifndef PROCESSHELPER_H
#define PROCESSHELPER_H

#include <string>

namespace process {
namespace helper {

#ifdef _WIN32
typedef std::wstring dir_string_t;
#else
typedef std::string dir_string_t;
#endif

class PushDir {
private:
	dir_string_t cwd_;
	static void chdir(dir_string_t const &dir);

public:
	PushDir() = default;
	PushDir(dir_string_t const &dir);
	~PushDir();
	void pushd(dir_string_t const &dir);
	void popd();
};

}
} // namespace process::helper

#endif // PROCESSHELPER_H
