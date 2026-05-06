#ifndef GITCOMMANDCACHE_H
#define GITCOMMANDCACHE_H

#include <QString>
#include <map>
#include <memory>
#include <vector>

class GitCommandCache {
public:
private:
	struct Data {
		std::map<std::string, std::vector<char>> map;
	};
	std::shared_ptr<Data> d;
public:
	GitCommandCache();

	std::vector<char> *find(std::string const &key);
	void insert(const std::string &key, std::vector<char> const &value);

	void clear();
};


#endif // GITCOMMANDCACHE_H
