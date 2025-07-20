#ifndef GZIP_H
#define GZIP_H

#include <string>
#include <vector>
#include "common/AbstractSimpleIO.h"

class gzip {
private:
	std::string error_;
	bool header_only_ = false;
	int64_t max_size_ = -1;
public:
	static bool is_valid_gz_file(AbstractSimpleReader *input);
	void set_header_only(bool f);
	void set_maximul_size(int64_t size);
	bool decompress(AbstractSimpleReader *input, AbstractSimpleWriter *output);
	std::string error() const
	{
		return error_;
	}
};

#endif // GZIP_H
