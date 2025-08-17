#include "GitCommandCache.h"

GitCommandCache::GitCommandCache()
{
	d = std::make_shared<Data>();
}

std::vector<char> *GitCommandCache::find(const QString &key)
{
	if (!d) return nullptr;
	auto it = d->map.find(key);
	if (it != d->map.end()) {
		return &it->second;
	}
	return nullptr;
}

void GitCommandCache::insert(const QString &key, const std::vector<char> &value)
{
	if (!d) return;
	d->map[key] = value;
}

void GitCommandCache::clear()
{
	if (d) {
		d->map.clear();
	}
}
