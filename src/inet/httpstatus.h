#ifndef __HTTPSTATUS_H
#define __HTTPSTATUS_H

#include <string_view>

struct http_status_t {
	int const code;
	char const *text;
};

#define HTTP_STATUS(CODE, KEY, TEXT) extern http_status_t http##CODE##KEY[1];
#include "httpstatus.txt"
#undef HTTP_STATUS

std::string_view http_status_text(int code);

#endif
