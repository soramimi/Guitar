#include "wstring.h"
#include <Windows.h>

std::wstring misc::convert_str_to_wstr(std::string const &str)
{
	std::wstring wstr;
	if (str.empty()) return wstr;
	int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
	if (len > 0) {
		wstr.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], len);
	}
	return wstr;
}

std::string misc::convert_wstr_to_str(std::wstring const &str)
{
	std::string s;
	if (str.empty()) return s;
	int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0, nullptr, nullptr);
	if (len > 0) {
		s.resize(len);
		WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), &s[0], len, nullptr, nullptr);
	}
	return s;
}


