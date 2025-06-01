
// #include "FileType2.h"
// #include "MemoryReader.h"
// #include "gunzip.h"
// #include <magic.h>
// #include <QDebug>
// #include <QFile>
// #include <QBuffer>
// #include <string>
// #include "common/misc.h"

// #if 0
// FileType2::FileType2()
// {
// 	//open(); // このインスタンスを作成した側でopen()を呼ぶこと
// }

// FileType2::~FileType2()
// {
// 	close();
// }

// bool FileType2::open(char const *mgcdata, size_t mgcsize)
// {
// 	magic_cookie = magic_open(MAGIC_MIME);

// 	if (magic_cookie == nullptr) {
// 		fprintf(stderr, "unable to initialize magic library\n");
// 		return false;
// 	}

// 	mgcdata_.resize(mgcsize);
// 	memcpy(mgcdata_.data(), mgcdata, mgcsize);
// 	void *bufs[1];
// 	size_t sizes[1];
// 	bufs[0] = mgcdata_.data();
// 	sizes[0] = mgcdata_.size();
// 	auto r = magic_load_buffers(magic_cookie, bufs, sizes, 1);
// 	return r == 0;
// }

// void FileType2::close()
// {
// 	if (magic_cookie) {
// 		magic_close(magic_cookie);
// 		magic_cookie = nullptr;
// 	}
// }

// std::string FileType2::determine(const char *bin, int len) const
// {
// 	Q_ASSERT(magic_cookie); // not called open() yet or failed to load magic file

// 	if (!bin || len < 1) return {};
// 	auto *p = magic_buffer(magic_cookie, bin, len);
// 	if (!p) return {};
// 	std::string s = p;
// 	auto i = s.find(';');
// 	if (i != std::string::npos) {
// 		s = s.substr(0, i);
// 	}
// 	return std::string{misc::trimmed(s)};
// }
// #endif


