#ifndef SIMPLEQTIO_H
#define SIMPLEQTIO_H

#include <common/AbstractSimpleIO.h>
#include <QIODevice>

class SimpleQtReader : public AbstractSimpleReader {
private:
	QIODevice *input_;
public:
	SimpleQtReader(QIODevice *input)
		: input_(input)
	{
		if (!input_) {
			throw std::runtime_error("input device is null");
		}
	}
	int read(void *ptr, int64_t len) override
	{
		if (!input_) {
			throw std::runtime_error("input device is null");
		}
		if (len <= 0) {
			return 0;
		}
		return input_->read(static_cast<char *>(ptr), len);
	}
	int64_t pos() const override
	{
		if (!input_) {
			throw std::runtime_error("input device is null");
		}
		return input_->pos();
	}
	void seek(int64_t pos) override
	{
		if (!input_) {
			throw std::runtime_error("input device is null");
		}
		input_->seek(pos);
	}
};

class SimpleQtWriter : public AbstractSimpleWriter {
private:
	QIODevice *output_;
public:
	SimpleQtWriter(QIODevice *output)
		: output_(output)
	{
		if (!output_) {
			throw std::runtime_error("output device is null");
		}
	}
	int write(void const *ptr, int64_t len) override
	{
		if (!output_) {
			throw std::runtime_error("output device is null");
		}
		if (len <= 0) {
			return 0;
		}
		return output_->write(static_cast<const char *>(ptr), len);
	}
};

#endif // SIMPLEQTIO_H
