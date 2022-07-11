#ifndef REPOSITORYSEARCHRESULTITEM_H
#define REPOSITORYSEARCHRESULTITEM_H

#include <string>

struct RepositorySearchResultItem {
	std::string full_name;
	std::string description;
	std::string ssh_url;
	std::string clone_url;
	std::string html_url;
	double score = 0;
};

#endif // REPOSITORYSEARCHRESULTITEM_H
