#include "tzst.h"
#include "joinpath.h"
#include "misc.h"
#include "tar.h"
#include "zs.h"
#include <cstdint>
#include <deque>
#include <fcntl.h>
#include <functional>
#include <mutex>
#include <cstdio>
#include <sys/stat.h>
#include <thread>
#include <condition_variable>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#define O_BINARY (0)
#endif

bool tzst::archive_tar_zst(std::string const &archive_path, std::string const &src_dir, std::string const &dst_prefix_dir)
{
	int fd_tarzst_out = open(archive_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	if (fd_tarzst_out == -1) {
		fprintf(stderr, "Could not create file: %s", archive_path.c_str());
		return false;
	}

	std::mutex mutex;
	std::condition_variable cond;

	std::deque<char> tar_buffer;

	bool tar_eof = false;

	std::thread zst_th([&](){
		auto In = [&](char *ptr, int len)->int{
			int total = 0;
			while (len > 0) {
				std::unique_lock lock(mutex);
				if (tar_buffer.empty()) {
					if (tar_eof) break;
					cond.wait(lock);
				} else {
					int n = std::min((int)tar_buffer.size(), len);
					std::copy(tar_buffer.begin(), tar_buffer.begin() + n, ptr);
					tar_buffer.erase(tar_buffer.begin(), tar_buffer.begin() + n);
					ptr += n;
					len -= n;
					total += n;
				}
			}
			return total;
		};
		auto Out = [&](char const *ptr, int len)->int{
			write(fd_tarzst_out, ptr, len);
			return len;
		};

		ZS zs;
		zs.compress(In, Out);
	});

	tar::TarWriter tar([&](char const *ptr, int len)->int{
		std::lock_guard lock(mutex);
		tar_buffer.insert(tar_buffer.end(), ptr, ptr + len);
		cond.notify_all();
		return len;
	});
	tar.archive(src_dir, dst_prefix_dir);
	tar_eof = true;
	cond.notify_all();

	zst_th.join();
	close(fd_tarzst_out);
	return true;
}

bool tzst::extract_tar_zst(std::string const &tarzst_path, std::string const &dstdir)
{
	int fd_in = open(tarzst_path.c_str(), O_RDONLY | O_BINARY);
	if (fd_in == -1) return false;

	std::deque<char> tarbuf;
	bool tar_eof = false;
	std::mutex mutex;
	std::condition_variable cond;
	tar::TarReader r([&](char *ptr, int len)->int{
		int pos = 0;
		while (pos < len) {
			std::unique_lock lock(mutex);
			int n = std::min(len - pos, (int)tarbuf.size());
			if (n > 0) {
				std::copy(tarbuf.begin(), tarbuf.begin() + n, ptr + pos);
				tarbuf.erase(tarbuf.begin(), tarbuf.begin() + n);
				pos += n;
			} else {
				if (tar_eof) break;
				cond.wait(lock);
			}
		}
		return pos;
	});

	std::thread th([&](){
		r.extract(dstdir);
	});

	ZS zs;
	zs.decompress([&](char *ptr, int len){
		return read(fd_in, ptr, len);
	}, [&](char const *ptr, int len){
		std::lock_guard lock(mutex);
		tarbuf.insert(tarbuf.end(), ptr, ptr + len);
		cond.notify_all();
		return len;
	});
	tar_eof = true;
	cond.notify_all();
	th.join();
	close(fd_in);
	return true;
}

