#include "FileTypeDetector.h"
#include "gzip.h"
#include <cstring>

namespace {

class MemoryReader : public AbstractSimpleReader {
private:
	const char *data_ = nullptr;
	int64_t size_ = 0;
	int64_t offset_ = 0;
public:
	MemoryReader(const char *ptr = nullptr, int64_t len = 0)
		: data_(ptr)
		, size_(len)
	{
	}
public:
	int read(void *ptr, int64_t len)
	{
		if (offset_ >= size_) return 0;
		size_t n = std::min(len, size_ - offset_);
		memcpy(ptr, data_ + offset_, n);
		offset_ += n;
		return (int)n;
	}
	int64_t pos() const
	{
		return offset_;
	}
	void seek(int64_t pos)
	{
		if (pos < 0) pos = 0;
		if (pos > (int64_t)size_) pos = size_;
		offset_ = pos;
	}
};

class BufferWriter : public AbstractSimpleWriter {
private:
	std::vector<char> vec_;
public:
	int write(const void *ptr, int64_t len)
	{
		const char *p = (const char *)ptr;
		vec_.insert(vec_.end(), p, p + len);
		return (int)len;
	}
	std::vector<char> &buffer()
	{
		return vec_;
	}
	std::vector<char> const &buffer() const
	{
		return vec_;
	}
};

} // namespace

std::string FileTypeDetector::mimetype_by_data(const char *data, int64_t size) const
{
	if (!data || size == 0) return {};

	if (size > 10) {
		if (memcmp(data, "\x1f\x8b\x08", 3) == 0) { // gzip
			MemoryReader src(data, size);
			BufferWriter dst;

			gzip z;
			z.set_maximul_size(100000);
			z.decompress(&src, &dst);

			std::vector<char> const &unzipped = dst.buffer();
			if (!unzipped.empty()) {
				return detect(unzipped.data(), unzipped.size()).mimetype;
			}
		}
	}

	return detect(data, size).mimetype;
}

std::string FileTypeDetector::mimetype_by_file(const char *path) const
{
	FILE *f = fopen(path, "rb");
	if (!f) return {};
	char buf[8192];
	size_t n = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	if (n == 0) return {};
	return mimetype_by_data(buf, n);
}
