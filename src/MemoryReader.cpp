#include "MemoryReader.h"

MemoryReader::MemoryReader(char const *ptr, qint64 len)
{
	setData(ptr, len);
}

void MemoryReader::setData(char const *ptr, qint64 len)
{
	begin = ptr;
	end = begin + len;
}

bool MemoryReader::isSequential() const
{
	return false;
}

bool MemoryReader::open(OpenMode mode)
{
	mode |= QIODevice::Unbuffered;
	return QIODevice::open(mode | QIODevice::Unbuffered);
}

qint64 MemoryReader::pos() const
{
	return QIODevice::pos();
}

qint64 MemoryReader::size() const
{
	if (begin && begin < end) {
		return end - begin;
	}
	return 0;
}

bool MemoryReader::seek(qint64 pos)
{
	return QIODevice::seek(pos);
}

bool MemoryReader::atEnd() const
{
	return QIODevice::atEnd();
}

bool MemoryReader::reset()
{
	if (begin && begin < end) {
		return true;
	}
	return false;
}

qint64 MemoryReader::bytesToWrite() const
{
	return 0;
}

bool MemoryReader::canReadLine() const
{
	return bytesAvailable() > 0;
}

bool MemoryReader::waitForReadyRead(int /*msecs*/)
{
	return bytesAvailable() > 0;
}

bool MemoryReader::waitForBytesWritten(int /*msecs*/)
{
	return false;
}

qint64 MemoryReader::readData(char *data, qint64 maxlen)
{
	qint64 n = bytesAvailable();
	if (n > 0) {
		if (n > maxlen) {
			n = maxlen;
		}
		memcpy(data, begin + pos(), n);
	}
	return n;
}

qint64 MemoryReader::writeData(const char * /*data*/, qint64 /*len*/)
{
	return 0;
}
