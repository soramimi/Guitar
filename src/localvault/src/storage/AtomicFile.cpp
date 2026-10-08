#include "AtomicFile.h"

#include <cstdint>
#include <fstream>
#include <random>
#include <sstream>
#include <system_error>

#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace localvault {

namespace {

	std::string generateTempSuffix()
	{
		std::random_device rd;
		std::uniform_int_distribution<uint32_t> dist;
		std::ostringstream oss;
		oss << ".tmp" << std::hex << dist(rd);
		return oss.str();
	}

#if defined(__unix__) || defined(__APPLE__)

	bool writeAll(int fd, const char *data, size_t size)
	{
		while (size > 0) {
			const ssize_t n = ::write(fd, data, size);
			if (n < 0) {
				if (errno == EINTR) continue;
				return false;
			}
			data += n;
			size -= static_cast<size_t>(n);
		}
		return true;
	}

	void syncDirectory(const std::filesystem::path &directory)
	{
		const int fd = ::open(directory.empty() ? "." : directory.c_str(), O_RDONLY | O_CLOEXEC);
		if (fd < 0) return;
		::fsync(fd);
		::close(fd);
	}

#endif

} // namespace

namespace AtomicFile {

	StorageStatus write(const std::filesystem::path &path, Blob const &data)
	{
		const std::filesystem::path tempPath = path.string() + generateTempSuffix();

#if defined(__unix__) || defined(__APPLE__)
		// 作成時点で 0600 とし、他ユーザーから読める瞬間を作らない
		const int fd = ::open(tempPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
		if (fd < 0) return StorageStatus::Error;
		bool ok = writeAll(fd, data.data(), data.size()) && ::fsync(fd) == 0;
		ok = (::close(fd) == 0) && ok;
		if (!ok || ::rename(tempPath.c_str(), path.c_str()) != 0) {
			::unlink(tempPath.c_str());
			return StorageStatus::Error;
		}
		syncDirectory(path.parent_path());
		return StorageStatus::Ok;
#elif defined(_WIN32)
		HANDLE h = ::CreateFileW(tempPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return StorageStatus::Error;
		bool ok = true;
		const char *p = data.data();
		size_t remaining = data.size();
		while (ok && remaining > 0) {
			const DWORD chunk = static_cast<DWORD>(remaining > 0x40000000 ? 0x40000000 : remaining);
			DWORD written = 0;
			ok = ::WriteFile(h, p, chunk, &written, nullptr) && written == chunk;
			p += written;
			remaining -= written;
		}
		ok = ok && ::FlushFileBuffers(h);
		ok = ::CloseHandle(h) && ok;
		// std::filesystem::rename は実装によって既存ファイルを置き換えないため直接呼ぶ
		if (!ok || !::MoveFileExW(tempPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
			::DeleteFileW(tempPath.c_str());
			return StorageStatus::Error;
		}
		return StorageStatus::Ok;
#else
		{
			std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
			if (file && !data.empty()) {
				file.write(data.data(), static_cast<std::streamsize>(data.size()));
			}
			file.flush();
			if (!file) {
				std::error_code ec;
				std::filesystem::remove(tempPath, ec);
				return StorageStatus::Error;
			}
		}
		std::error_code ec;
		std::filesystem::rename(tempPath, path, ec);
		if (ec) {
			std::error_code removeEc;
			std::filesystem::remove(tempPath, removeEc);
			return StorageStatus::Error;
		}
		return StorageStatus::Ok;
#endif
	}

	StorageStatus read(const std::filesystem::path &path, Blob *out, size_t maxSize)
	{
		if (!out) return StorageStatus::Error;
		out->clear();
		std::error_code ec;
		const auto status = std::filesystem::status(path, ec);
		if (status.type() == std::filesystem::file_type::not_found) return StorageStatus::NotFound;
		if (ec || status.type() != std::filesystem::file_type::regular) return StorageStatus::Error;

		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file) return StorageStatus::Error;
		const auto size = file.tellg();
		if (size < 0 || static_cast<unsigned long long>(size) > maxSize) return StorageStatus::Error;
		file.seekg(0, std::ios::beg);

		Blob data(static_cast<size_t>(size));
		if (size > 0 && !file.read(data.data(), size)) return StorageStatus::Error;
		*out = std::move(data);
		return StorageStatus::Ok;
	}

	StorageStatus remove(const std::filesystem::path &path)
	{
		std::error_code ec;
		if (std::filesystem::remove(path, ec)) return StorageStatus::Ok;
		return ec ? StorageStatus::Error : StorageStatus::NotFound;
	}

} // namespace AtomicFile

} // namespace localvault
