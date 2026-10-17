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
		FileTypeWrapper::Result *result_ = nullptr;
	public:
		~Result()
		{
			if (result_) {
				void (*_free)(void *p) = result_->_free;
				if (_free) {
					_free(result_->mimetype);
					_free(result_->charset);
				}
				_free(result_);
			}
		}
		Result() = default;
		Result(FileTypeWrapper::Result *result)
			: result_(result)
		{
		}
		Result(Result const &) = delete;
		Result &operator=(Result const &) = delete;
		Result(Result &&t)
			: result_(t.result_)
		{
			t.result_ = nullptr;
		}
		Result &operator=(Result &&t)
		{
			if (this != &t) {
				result_ = t.result_;
				t.result_ = nullptr;
			}
			return *this;
		}
		bool isdir() const
		{
			return result_ ? result_->isdir : false;
		}
		const char *mimetype() const
		{
			return result_ ? result_->mimetype : nullptr;
		}
		const char *charset() const
		{
			return result_ ? result_->charset : nullptr;
		}
	};
	
	virtual bool open();
	virtual Result detect(char const *filepath) const;
	virtual Result detect(const char *data, size_t size, int filemode = 0644) const;
};

#endif // FILETYPE_H
