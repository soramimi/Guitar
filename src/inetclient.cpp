#include "inetclient.h"
#include <cstring>

InetClient::URL::URL(const std::string &addr)
{
	data_.full_request = addr;

	// Initialize with defaults
	data_.scheme = "http";
	data_.host = "";
	data_.port = 0;
	data_.path = "/";

	if (addr.empty()) {
		return;
	}

	char const *str = addr.c_str();
	char const *left = str;
	char const *right = strstr(left, "://");

	// Parse scheme (http, https, etc.)
	if (right) {
		if (right > left) { // Ensure scheme isn't empty
			data_.scheme.assign(str, right - str);
			left = right + 3;
		} else {
			left = right + 3; // Skip "://" but use default scheme
		}
	}

	// Find start of path, or end of string if no path
	right = strchr(left, '/');
	if (!right) {
		right = left + strlen(left);
	}

	// Handle host:port format
	if (left < right) {
		// Look for port separator
		char const *p = strchr(left, ':');
		if (p && left < p && p < right) {
			// Extract hostname
			data_.host.assign(left, p - left);

			// Parse port number
			int n = 0;
			char const *q = p + 1;
			bool port_valid = true;
			size_t digits = 0;

			while (q < right) {
				if (isdigit(*q & 0xff)) {
					int old_n = n;
					n = n * 10 + (*q - '0');

					// Check for integer overflow
					if (n < old_n || digits > 5) {
						port_valid = false;
						break;
					}
					digits++;
				} else {
					port_valid = false;
					break;
				}
				q++;
			}

			if (port_valid && n > 0 && n < 65536) {
				data_.port = n;
			}
		} else {
			// No port specified, just hostname
			data_.host.assign(left, right - left);
		}
	}

	// Extract path
	if (*right) {
		data_.path = right;
	}
}

bool InetClient::URL::is_ssl() const
{
	if (scheme() == "https") return true;
	if (scheme() == "http") return false;
	if (port() == 443) return true;
	return false;
}
