
#include "httpstatus.h"

#define HTTP_STATUS(CODE, KEY, TEXT) http_status_t http##CODE##KEY[1] = { CODE, TEXT };
#include "httpstatus.txt"
#undef HTTP_STATUS

std::string_view http_status_text(int code)
{
	switch (code) {
#define HTTP_STATUS(CODE, KEY, TEXT) case CODE: return TEXT;
#include "httpstatus.txt"
#undef HTTP_STATUS
	}
	return {};
}
