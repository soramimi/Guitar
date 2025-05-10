#ifndef FILETYPE_H
#define FILETYPE_H

#include <magic.h>
#include <QByteArray>

class FileType {
private:
	QByteArray mgcdata_;
	magic_t magic_cookie = nullptr;
public:
	FileType();
	~FileType();
	bool open();
	void close();
	std::string mime_by_data(char const *bin, int len) const;
	std::string mime_by_file(const QString &path);

	std::string determin(const QByteArray &in) const;
	std::string determin(const QString &path) const;
};

#endif // FILETYPE_H
