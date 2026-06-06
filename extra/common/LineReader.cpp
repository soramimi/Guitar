
#include "LineReader.h"
#include "Logger.h"
#include "common/realpath.h"
#include <common/joinpath.h>
#include <common/misc.h>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>
#include <io.h>
#else
#include <unistd.h>
#endif

LineReader::File *LineReader::current()
{
	return &files_.back();
}

void LineReader::close_all()
{
	for (size_t i = files_.size(); i > 0; i--) {
		File const &file = files_[i - 1];
		::close(file.fd);
	}
	files_.clear();
}

void LineReader::close_one()
{
	if (!files_.empty()) {
		::close(files_.back().fd);
		files_.pop_back();
	}
}

bool LineReader::internal_getline(std::string *out)
{
	File *f = nullptr;
	std::string line;
	while (1) {
		if (files_.empty()) return false;
		f = current();
		if (f->length > 0) {
			if (f->last_char == '\r') {
				if (f->buffer[f->offset] == '\n') {
					f->offset++;
					f->length--;
				}
				f->last_char = 0;
			}
			std::string_view v(f->buffer + f->offset, f->length);
			auto i = v.find_first_of("\r\n");
			if (i != std::string_view::npos) {
				f->last_char = v[i];
				line += std::string(v.substr(0, i));
				i++;
				f->offset += i;
				f->length -= i;
				break;
			}
			line += v;
		} else if (f->eof) {
			close_one();
			continue;
		}
		f->offset = 0;
		f->length = 0;
		int n = read(f->fd, f->buffer, sizeof(f->buffer));
		if (n < 1) {
			f->eof = true;
			break;
		}
		f->length = n;
	}
	if (out) {
		*out = line;
	}
	f->line++;
	return true;
}

LineReader::LineReader() = default;

LineReader::~LineReader()
{
	close_all();
}

const std::string &LineReader::file() const
{
	if (files_.empty()) return {};
	return files_.back().path;
}

int LineReader::line() const
{
	if (files_.empty()) return 0;
	return files_.back().line;
}

bool LineReader::open(const std::string &path)
{
	std::string abspath = misc::realpath(path);
	int fd = ::open(abspath.c_str(), O_RDONLY);
	if (fd != -1) {
		logprintf(LOG_BOTH, "load config file: %s\n", abspath.c_str());
		File file;
		file.path = path;
		file.fd = fd;
		file.line = 0;
		files_.push_back(file);
		return true;
	}
	return false;
}

bool LineReader::getline(std::string *out)
{
	while (1) {
		if (!internal_getline(out)) break;
		char const *left = out->c_str();
		char const *right = left + out->size();
		while (left < right && isspace((unsigned char)*left)) left++;
		while (right > left && isspace((unsigned char)right[-1])) right--;
		if (left + 7 < right && strncmp(left, "include", 7) == 0 && isspace((unsigned char)left[7])) {
			char const *mid = left + 7;
			while (mid < right && isspace((unsigned char)*mid)) mid++;
			if (mid < right) {
				std::string path;
				if (*mid == '/') {
					path = std::string(mid, right - mid);
				} else {
					std::string dir = current()->path;
					char const *slash = strrchr(dir.c_str(), '/');
					if (slash) {
						dir = dir.substr(0, slash - dir.c_str() + 1);
					} else {
						dir = {};
					}
					path = dir + std::string(mid, right - mid);
				}
				open(path);
				continue;
			}
			//
		} else {
			return true;
		}
	}
	return false;
}
