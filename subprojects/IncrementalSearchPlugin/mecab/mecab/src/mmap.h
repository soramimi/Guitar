// MeCab -- Yet Another Part-of-Speech and Morphological Analyzer
//
//
//  Copyright(C) 2001-2006 Taku Kudo <taku@chasen.org>
//  Copyright(C) 2004-2006 Nippon Telegraph and Telephone Corporation
#ifndef MECAB_MMAP_H
#define MECAB_MMAP_H

#include <errno.h>
#include <string>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif
#include <io.h>
#else

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif
}

#include "common.h"
#include "utils.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

bool x_get_resource_i8(char const *name, std::vector<char> *buf);
bool x_get_resource_i16(char const *name, std::vector<short int> *buf);

template <typename T> bool _t_get_resource(char const *name, std::vector<T> *buf);
template <> inline bool _t_get_resource<short int>(char const *name, std::vector<short int> *buf)
{
	return x_get_resource_i16(name, buf);
}

template <> inline bool _t_get_resource<char>(char const *name, std::vector<char> *buf)
{
	return x_get_resource_i8(name, buf);
}

namespace MeCab {

template <class T> class Mmap {
private:
	std::vector<T> text;
	size_t       length;
	std::string  fileName;
	whatlog what_;

	int    fd;
	int    flag;

public:
	T&       operator[](size_t n)       { return *(text.data() + n); }
	const T& operator[](size_t n) const { return *(text.data() + n); }
	T*       begin()           { return text.data(); }
	const T* begin()    const  { return text.data(); }
	T*       end()           { return text.data() + size(); }
	const T* end()    const  { return text.data() + size(); }
	size_t size()               { return length/sizeof(T); }
	const char *what()          { return what_.str(); }
	const char *file_name()     { return fileName.c_str(); }
	size_t file_size()          { return length; }
	bool empty()                { return(length == 0); }

	bool open(const char *filename)
	{
		this->close();

		// logprintf("--- processing %s ---\n", filename);

		if (_t_get_resource(filename, &text)) {
			fileName = filename;
			length = text.size() * sizeof(T);

			return true;
		}

		struct stat st;
		fileName = std::string(filename);

		// if (std::strcmp(mode, "r") == 0) {
			flag = O_RDONLY;
		// } else if (std::strcmp(mode, "r+") == 0) {
		// 	flag = O_RDWR;
		// } else {
		// 	CHECK_FALSE(false) << "unknown open mode: " << filename;
		// }

		CHECK_FALSE((fd = ::open(filename, flag | O_BINARY)) >= 0)
				<< "open failed: " << filename;

		CHECK_FALSE(::fstat(fd, &st) >= 0)
				<< "failed to get file size: " << filename;

		length = st.st_size;

		text.resize(length);
		CHECK_FALSE(::read(fd, text.data(), length) >= 0)
				<< "read() failed: " << filename;
		::close(fd);
		fd = -1;

		return true;
	}

	void close()
	{
		if (fd >= 0) {
			::close(fd);
			fd = -1;
		}

		if (!text.empty()) {
			if (flag == O_RDWR) {
				int fd2;
				if ((fd2 = ::open(fileName.c_str(), O_RDWR)) >= 0) {
					::write(fd2, text.data(), length);
					::close(fd2);
				}
			}
		}

		text.clear();
	}

	Mmap()
		: fd(-1)
	{
	}

	virtual ~Mmap() { this->close(); }
};
}
#endif
