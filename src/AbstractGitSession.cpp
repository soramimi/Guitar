#include "AbstractGitSession.h"
#include "GitCommandCache.h"
#include "common/joinpath.h"

struct AbstractGitSession::GitCache {
	GitCommandCache command_cache;
};

struct AbstractGitSession::Private {
	std::shared_ptr<AbstractGitSession::GitCache> cache;
	AbstractGitSession::Info info;
	AbstractGitSession::Info2 info2;
};

void AbstractGitSession::insertIntoCommandCache(const QString &key, const std::vector<char> &value)
{
	m->cache->command_cache.insert(key, value);
}

std::vector<char> *AbstractGitSession::findFromCommandCache(const QString &key)
{
	return m->cache->command_cache.find(key);
}

AbstractGitSession::AbstractGitSession()
	: m(new Private)
{
	m->cache = std::make_shared<GitCache>();
}

AbstractGitSession::~AbstractGitSession()
{
	delete m;
}

AbstractGitSession::AbstractGitSession(const AbstractGitSession &other)
	: m(new Private(*other.m))
{
}

void AbstractGitSession::clearCommandCache()
{
	m->cache->command_cache.clear();
}

AbstractGitSession::Info &AbstractGitSession::gitinfo()
{
	return m->info;
}

const AbstractGitSession::Info &AbstractGitSession::gitinfo() const
{
	return m->info;
}

AbstractGitSession::Info2 &AbstractGitSession::gitinfo2()
{
	return m->info2;
}

const AbstractGitSession::Info2 &AbstractGitSession::gitinfo2() const
{
	return m->info2;
}

AbstractGitSession::GitCache &AbstractGitSession::cache()
{
	return *m->cache;
}

QString AbstractGitSession::workingDir() const
{
	QString dir = gitinfo2().working_repo_dir;
	if (!gitinfo2().submodule_path.isEmpty()) {
		dir = dir / gitinfo2().submodule_path;
	}
	return dir;
}


