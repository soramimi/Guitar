#ifndef FILETYPE_H
#define FILETYPE_H

#include <magic.h>
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
	QString mime_by_data(char const *bin, int len);
	QString mime_by_file(const QString &path);
};

#endif // FILETYPE_H
