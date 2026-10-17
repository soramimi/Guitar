#ifndef FILETYPE_H
#define FILETYPE_H

#include <QObject>
#include "FileTypeWrapper.h"

class FileType : public QObject {
	Q_OBJECT
private:
	struct Private;
	Private *m;
public:
	FileType();
	~FileType();
	
	class Result {
	private:
		// FileTypeWrapper::Result *result_ = nullptr;
	public:
		bool isdir = false;
		QString mimetype;
		QString charset;
		// ~Result()
		// {
		// 	if (result_) {
		// 		void (*_free)(void *p) = result_->_free;
		// 		if (_free) {
		// 			_free(result_->mimetype);
		// 			_free(result_->charset);
		// 		}
		// 		_free(result_);
		// 	}
		// }
		Result() = default;
		Result(FileTypeWrapper::Result *result)
			// : result_(result)
		{
			if (result) {
				isdir = result->isdir;
				mimetype = QString::fromUtf8(result->mimetype);
				charset = QString::fromUtf8(result->charset);
				void (*_free)(void *p) = result->_free;
				if (_free) {
					_free(result->mimetype);
					_free(result->charset);
					_free(result);
				}
			}
		}
	};
	
	virtual bool open();
	virtual Result detect(char const *filepath) const;
	virtual Result detect(const char *data, size_t size, int filemode = 0644) const;
};

#endif // FILETYPE_H
