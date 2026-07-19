


#ifndef WSTRING_H
#define WSTRING_H

#include <string>

namespace misc {

#ifdef _WIN32
std::wstring convert_str_to_wstr(std::string_view const &str);
std::string convert_wstr_to_str(std::wstring_view const &str);
#else
std::u16string convert_str_to_wstr(std::string_view const &str);
std::string convert_wstr_to_str(std::u16string_view const &str);
#endif

}

#endif // WSTRING_H
