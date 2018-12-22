#ifndef MEMORYREADER_H
#define MEMORYREADER_H

#include <QBuffer>
#include <QIODevice>

class MemoryReader : public QIODevice {
private:
	char const *begin;
	char const *end;
public:
	MemoryReader(char const *ptr = nullptr, qint64 len = 0);
	void setData(char const *ptr, qint64 len);
	bool isSequential() const override;
	bool open(OpenMode mode) override;
	qint64 pos() const override;
	qint64 size() const override;
	bool seek(qint64 pos) override;
	bool atEnd() const override;
	bool reset() override;
	qint64 bytesToWrite() const override;
	bool canReadLine() const override;
	bool waitForReadyRead(int msecs) override;
	bool waitForBytesWritten(int msecs) override;
protected:
	qint64 readData(char *data, qint64 maxlen) override;
	qint64 writeData(char const *data, qint64 len) override;
};

#endif // MEMORYREADER_H
