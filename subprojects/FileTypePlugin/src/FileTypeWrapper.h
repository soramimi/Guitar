#ifndef FILETYPEWRAPPER_H
#define FILETYPEWRAPPER_H

#include <mutex>
#include <string>
#include <vector>

struct InternalFileTypeResult {
	void (*_free)(void *p) = nullptr;
	
	InternalFileTypeResult() = default;
	~InternalFileTypeResult()
	{
		if (_free) {
			_free(mimetype);
			_free(charset);
		}
	}
	bool isdir = false;
	char *mimetype = nullptr;
	char *charset = nullptr;
};

class FileTypeWrapper {
private:
	mutable std::mutex mutex_; // libfileはマルチスレッドに脆弱なようだ。
	void *magic_set_ = nullptr;
	std::vector<char> mgcdata_;
	bool open();
	void close();
public:
	FileTypeWrapper()
	{
		open();
	}
	~FileTypeWrapper()
	{
		close();
	}
	static size_t slop_size();
	InternalFileTypeResult *detect(int fd) const;
	InternalFileTypeResult *detect(char const *filepath) const;
	InternalFileTypeResult *detect(const char *data, size_t size, int filemode = 0644) const;
};

#endif // FILETYPEWRAPPER_H
