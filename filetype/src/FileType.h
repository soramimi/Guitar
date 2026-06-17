#ifndef FILETYPE_H
#define FILETYPE_H

#include <mutex>
#include <string>
#include <vector>

class FileType {
public:
	struct Result {
		std::string mimetype;
		std::string charset;
	};
private:
	mutable std::mutex mutex_; // libfileはマルチスレッドに脆弱なようだ。
	void *magic_set_ = nullptr;
	std::vector<char> mgcdata_;
	bool open();
#if 0
	bool open(const char *mgcptr, size_t mgclen);
	bool open(char const *mgcfile);
#endif
	void close();
public:
	FileType()
	{
		open();
	}
	~FileType()
	{
		close();
	}
	static size_t slop_size();
	Result detect(int fd) const;
	Result detect(char const *filepath) const;
	Result detect(const char *data, size_t size, int filemode = 0644) const;
};

#ifdef _WIN32
#ifndef DLLEXPORT
#define DLLEXPORT __declspec(dllimport)
#endif
#else
#define DLLEXPORT
#endif

struct Hoge {
	struct Private;
	Private *m;
};

extern "C" DLLEXPORT void dtor(Hoge *self);
extern "C" DLLEXPORT Hoge hoge();
extern "C" DLLEXPORT int value(Hoge *self);

#endif // FILETYPE_H
