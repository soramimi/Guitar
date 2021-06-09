#ifndef FILETYPE_H
#define FILETYPE_H

#include <magic.h>
#include <string>
#include <QByteArray>

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

#endif // FILETYPE_H
