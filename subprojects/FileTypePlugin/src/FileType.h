#ifndef FILETYPE_H
#define FILETYPE_H

#include <QObject>

class InternalFileTypeResult;

class FileType : public QObject {
	Q_OBJECT
private:
	struct Private;
	Private *m;
public:
	FileType();
	~FileType();
	
	class Result {
	public:
		bool isdir = false;
		QString mimetype;
		QString charset;
		Result() = default;
		Result(const InternalFileTypeResult &result);
	};
	
	virtual bool open();
	virtual Result detect(char const *filepath) const;
	virtual Result detect(const char *data, size_t size, int filemode = 0644) const;
};

#endif // FILETYPE_H
