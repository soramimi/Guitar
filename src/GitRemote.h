
#ifndef GITREMOTE_H
#define GITREMOTE_H

#include <string>

struct GitRemote {
	std::string name;
	std::string url_fetch;
	std::string url_push;
	std::string ssh_key;
	bool operator < (GitRemote const &r) const
	{
		return name < r.name;
	}
	std::string const &url() const
	{
		return url_fetch;
	}
	void set_url(std::string const &url)
	{
		url_fetch = url;
		url_push = url;
	}
};

#endif // GITREMOTE_H
