#include "platform.h"
#include "ApplicationGlobal.h"

#ifndef Q_OS_WIN
extern char** environ;
#endif

namespace platform {

void initNetworking()
{
	std::string http_proxy;
	std::string https_proxy;
	if (global->appsettings.proxy_type == "auto") {
#ifdef Q_OS_WIN
		http_proxy = misc::makeProxyServerURL(getWin32HttpProxy().toStdString());
#else
		auto getienv = [](std::string const &name)->char const *{
			char **p = environ;
			while (*p) {
				if (strncasecmp(*p, name.c_str(), name.size()) == 0) {
					char const *e = *p + name.size();
					if (*e == '=') {
						return e + 1;
					}
				}
				p++;
			}
			return nullptr;
		};
		char const *p;
		p = getienv("http_proxy");
		if (p) {
			http_proxy = misc::makeProxyServerURL(std::string(p));
		}
		p = getienv("https_proxy");
		if (p) {
			https_proxy = misc::makeProxyServerURL(std::string(p));
		}
#endif
	} else if (global->appsettings.proxy_type == "manual") {
		http_proxy = global->appsettings.proxy_server.toStdString();
	}
	global->webcx.set_http_proxy(http_proxy);
	global->webcx.set_https_proxy(https_proxy);
}

} // namespace platform

