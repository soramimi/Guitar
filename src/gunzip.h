#ifndef GUNZIP_H
#define GUNZIP_H

#include <QIODevice>
#include <string>
#include <functional>
#include <cstdint>

class gunzip {
public:
	QString error;
	bool header_only = false;
	int64_t maxsize = -1;
	std::function<bool(QIODevice *)> open;
	std::function<void(QIODevice *)> close;
	std::function<bool(QIODevice *, char const *ptr, int len)> write;

	void set_header_only(bool f);
	void set_maximul_size(int64_t size);

	bool decode(QIODevice *input, QIODevice *output);
	bool decode(QString const &inpath, QString const &outpath);

	static bool is_valid_gz_file(QIODevice *input);
	static bool is_valid_gz_file(QString const &inpath);
};

#endif // GUNZIP_H
