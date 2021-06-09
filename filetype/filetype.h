#ifndef FILETYPE_H
#define FILETYPE_H

#include <string>

#if 0//def _WIN32

#include <filetype/file/src/magic.h>

magic_t filetype_open();
void filetype_close(magic_t magic_cookie);
std::string filetype_mime_by_data(magic_t magic_cookie, char const *bin, int len);

class FileType {
private:
	magic_t magic_cookie;
public:
	FileType()
	{
		open();
	}
	~FileType()
	{
		close();
	}
	bool open()
	{
		magic_cookie = filetype_open();
		return (bool)magic_cookie;
	}
	void close()
	{
		filetype_close(magic_cookie);
	}
	std::string mime_by_data(const char *bin, int len)
	{
		return filetype_mime_by_data(magic_cookie, bin, len);
	}
};

#else

#include <QByteArray>
#include <magic.h>

class FileType {
private:
	QByteArray mgcdata_;
	magic_t magic_cookie = nullptr;
public:
	FileType()
	{
//		open(); // このインスタンスを作成した側でopen()を呼ぶこと
	}
	~FileType()
	{
		close();
	}
	bool open();
	void close();
	std::string mime_by_data(char const *bin, int len);
};

#endif

#endif // FILETYPE_H
