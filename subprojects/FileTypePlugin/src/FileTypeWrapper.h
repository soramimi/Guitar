#ifndef FILETYPEWRAPPER_H
#define FILETYPEWRAPPER_H

#include <mutex>
#include <string>
#include <vector>

class FileTypeWrapper {
public:
	class Result {
	private:
	public:
		void (*_free)(void *p) = nullptr;
	public:
		Result() = default;
		~Result()
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
	Result *detect(int fd) const;
	Result *detect(char const *filepath) const;
	Result *detect(const char *data, size_t size, int filemode = 0644) const;
};

#endif // FILETYPEWRAPPER_H
