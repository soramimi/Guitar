#ifndef MEMORYREADER_H
#define MEMORYREADER_H

#include <QBuffer>
#include <QIODevice>

class MemoryReader : public QIODevice {
private:
	char const *begin;
	char const *end;
public:
	MemoryReader(char const *ptr = 0, qint64 len = 0);
	void setData(char const *ptr, qint64 len);
	virtual bool isSequential() const;
	virtual bool open(OpenMode mode);
	virtual qint64 pos() const;
	virtual qint64 size() const;
	virtual bool seek(qint64 pos);
	virtual bool atEnd() const;
	virtual bool reset();
	virtual qint64 bytesToWrite() const;
	virtual bool canReadLine() const;
	virtual bool waitForReadyRead(int msecs);
	virtual bool waitForBytesWritten(int msecs);
protected:
	virtual qint64 readData(char *data, qint64 maxlen);
	virtual qint64 writeData(const char *data, qint64 len);
};

#endif // MEMORYREADER_H
