#ifndef ABSTRACTSIMPLEIO_H
#define ABSTRACTSIMPLEIO_H

#include <cstddef>
#include <cstdint>

class AbstractSimpleReader {
public:
	virtual ~AbstractSimpleReader() = default;
	virtual int read(void *ptr, int64_t len) = 0;
	virtual int64_t pos() const = 0;
	virtual void seek(int64_t pos) = 0;
};

class AbstractSimpleWriter {
public:
	virtual ~AbstractSimpleWriter() = default;
	virtual int write(void const *ptr, int64_t len) = 0;
};

#endif // ABSTRACTSIMPLEIO_H
