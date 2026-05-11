
#ifndef _WIN32
#error This header is only for Windows. If you want to use it on other platforms, please implement the conversion functions for that platform.
#endif


#ifndef WSTRING_H
#define WSTRING_H

#include <string>

namespace misc {

std::wstring convert_str_to_wstr(std::string const &str);
std::string convert_wstr_to_str(std::wstring const &str);

}

#endif // WSTRING_H
