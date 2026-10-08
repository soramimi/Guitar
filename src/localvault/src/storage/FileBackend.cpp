#include "FileBackend.h"
#include "AtomicFile.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

#if defined(_WIN32)
#include <shlobj.h>
#include <windows.h>
#endif

namespace localvault {

FileBackend::FileBackend(const std::filesystem::path &directory)
{
	if (directory.empty()) {
		directory_ = defaultConfigDirectory() / "emk";
	} else {
		directory_ = directory;
	}
}

bool FileBackend::isAvailable() const
{
	return true;
}

std::string FileBackend::name() const
{
	return "file";
}

std::string FileBackend::sanitizeKey(std::string const &key)
{
	// ファイル名として許可する文字は英数字、ドット、ハイフン、アンダースコアのみ
	std::string safe;
	safe.reserve(key.size());
	for (const char c : key) {
		const bool allowed = (c >= 'A' && c <= 'Z')
			|| (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9')
			|| c == '.'
			|| c == '_'
			|| c == '-';
		safe.push_back(allowed ? c : '_');
	}
	if (safe.empty() || safe == "." || safe == "..") {
		safe = "_key_";
	}
	if (safe.size() > 128) {
		safe.resize(128);
	}
	return safe;
}

std::filesystem::path FileBackend::defaultConfigDirectory()
{
#if defined(_WIN32)
	wchar_t *path = nullptr;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path)) && path) {
		std::filesystem::path result(path);
		CoTaskMemFree(path);
		return result / "localvault";
	}
	const char *appdata = std::getenv("APPDATA");
	if (appdata) {
		return std::filesystem::path(appdata) / "localvault";
	}
	return std::filesystem::path(std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : ".") / "localvault";
#elif defined(__APPLE__)
	const char *home = std::getenv("HOME");
	if (home) {
		return std::filesystem::path(home) / "Library" / "Application Support" / "localvault";
	}
	return std::filesystem::path(".") / "localvault";
#else
	const char *xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
	if (xdgConfigHome) {
		return std::filesystem::path(xdgConfigHome) / "localvault";
	}
	const char *home = std::getenv("HOME");
	if (home) {
		return std::filesystem::path(home) / ".config" / "localvault";
	}
	return std::filesystem::path(".") / "localvault";
#endif
}

std::filesystem::path FileBackend::filePathForKey(std::string const &key) const
{
	return directory_ / sanitizeKey(key);
}

static bool ensureDirectory(const std::filesystem::path &directory)
{
	std::error_code ec;
	std::filesystem::create_directories(directory, ec);
	if (ec || !std::filesystem::is_directory(directory, ec)) {
		return false;
	}
#if defined(__unix__) || defined(__APPLE__)
	chmod(directory.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
#endif
	return true;
}

StorageStatus FileBackend::store(std::string const &key, Blob const &data)
{
	if (!ensureDirectory(directory_)) {
		return StorageStatus::Error;
	}
	return AtomicFile::write(filePathForKey(key), data);
}

StorageStatus FileBackend::load(std::string const &key, Blob *out)
{
	return AtomicFile::read(filePathForKey(key), out);
}

StorageStatus FileBackend::remove(std::string const &key)
{
	return AtomicFile::remove(filePathForKey(key));
}

} // namespace localvault
