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
		std::map<QString, std::vector<char>> map;
	};
	std::shared_ptr<Data> d;
public:
	GitCommandCache();

	std::vector<char> *find(QString const &key);
	void insert(QString const &key, std::vector<char> const &value);

	void clear();
};


#endif // GITCOMMANDCACHE_H
