#ifndef ZIP_H
#define ZIP_H

#include <string>

namespace zip {

class Zip {



public:
	static int archive_main();
	static bool extract_from_data(const char *zipdata, size_t zipsize, const std::string &destdir);
	static bool extract(const std::string &zipfile, const std::string &destdir);
	static int ziptest();
};

} // namespace zip

#endif // ZIP_H
