#include "realpath.h"
#include "misc.h"
#include "joinpath.h"

#ifdef _WIN32
#include <windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>
#include "common/wstring.h"
#else
#include <limits.h>
#endif

/**
 * @brief パスを絶対パスに変換する
 *
 * 指定されたパスを絶対パスに変換して返します。パスが存在しない場合や、変換に失敗した場合は空文字列を返します。
 * '~'で始まるパスは、環境変数HOMEの値を展開して変換します。
 *
 * @param path 変換するパス
 * @return 絶対パス。変換に失敗した場合は空文字列。
 */
std::string misc::realpath(const char *path)
{
#ifdef _WIN32
	std::string s = path;
	for (char &c : s) {
		if (c == '/') {
			c = '\\';
		}
	}
	if (*path == '~') {
		PWSTR path2 = NULL;
		HRESULT hr = SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &path2);

		if (SUCCEEDED(hr)) {
			s = misc::convert_wstr_to_str(path2);
			// fprintf(stderr, "Home Directory: %s\n", s.c_str());
			// Must free the memory allocated by the API
			CoTaskMemFree(path2);
		}

		s = s / (path + 1);
	}
	char tmp[MAX_PATH];
	if (_fullpath(tmp, s.c_str(), MAX_PATH)) {
		return misc::normalizePathSeparator(s);
	}
#else
	std::string s;
	if (*path == '~') {
		char const *home = getenv("HOME");
		if (home) {
			s = home;
			s = s / (path + 1);
			path = s.c_str();
		}
	}
	char tmp[PATH_MAX];
	char *p = ::realpath(path, tmp);
	if (p) {
		return tmp;
	}
#endif
	fprintf(stderr, "Warning: realpath failed for path: %s.\n", path);
	return {};
}

std::string misc::realpath(std::string const &path)
{
	return realpath(path.c_str());
}
