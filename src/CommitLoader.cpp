#include "CommitLoader.h"
#include "MemoryReader.h"
#include "json.h"
#include "webclient.h"

#include <QDebug>

namespace {
const int MAX_CACHE_COUNT = 1000;
}

typedef std::shared_ptr<WebClient> WebClientPtr;

struct CommitLoader::Private {
	QMutex data_mutex;
	QMutex thread_mutex;
	QWaitCondition condition;
	std::map<std::string, std::string> avatar_url_cache;
	std::deque<RequestItem> requested;
	std::deque<RequestItem> completed;
	std::set<std::string> notfound;
	WebContext *webcx = nullptr;
	WebClientPtr web1;
	WebClientPtr web2;
};

CommitLoader::CommitLoader()
	: m(new Private)
{
}

CommitLoader::~CommitLoader()
{
	delete m;
}

void CommitLoader::start(WebContext *webcx)
{
	m->webcx = webcx;
}

void CommitLoader::run()
{
	while (1) {
		if (isInterruptionRequested()) {
			break;
		}
		if (!m->web1) {
			m->web1 = WebClientPtr(new WebClient(m->webcx));
		}

		m->thread_mutex.lock();
		m->condition.wait(&m->thread_mutex);
		m->thread_mutex.unlock();

		if (isInterruptionRequested()) {
			break;
		}

		RequestItem item;

		{
			QMutexLocker lock(&m->data_mutex);
			if (!m->requested.empty()) {
				item = m->requested.front();
			}
		}

		if (item.url.empty()) goto L1;

		{
			auto it = m->avatar_url_cache.find(item.url);
			if (it == m->avatar_url_cache.end()) {
				if (m->web1->get(item.url) == 200) {
					JSON json;
					json.parse(&m->web1->response().content);
					for (JSON::Node const &node1 : json.node.children) {
						if (node1.name == "commit") {
							for (JSON::Node const &node2 : node1.children) {
								if (node2.name == "author") {
									for (JSON::Node const &node3 : node2.children) {
										if (node3.name == "name") {
											item.user.name = node3.value;
										} else if (node3.name == "email") {
											item.user.email= node3.value;
										}
									}
								}
							}
						} else if (node1.name == "author") {
							for (JSON::Node const &node2 : node1.children) {
								if (node2.name == "login") {
									item.user.login = node2.value;
								} else if (node2.name == "avatar_url") {
									item.user.avatar_url = node2.value;
								}
							}
						}
					}
					if (!item.user.login.empty() && !item.user.email.empty()) {
						{
							QMutexLocker lock(&m->data_mutex);
							while (m->completed.size() >= MAX_CACHE_COUNT) {
								m->completed.pop_back();
							}
							m->completed.push_front(item);
						}
						emit updated();
						goto L1;
					}
				}
			}

			{ // not found
				QMutexLocker lock(&m->data_mutex);
				m->notfound.insert(m->notfound.end(), item.url);
			}
		}
L1:;
		{
			QMutexLocker lock(&m->data_mutex);
			if (!m->requested.empty()) {
				m->requested.pop_front();
			}
		}
	}
}

GitHubAPI::User CommitLoader::fetch(QString const &url, bool request)
{
	RequestItem item;
	item.url = url.toStdString();
	if (!item.url.empty()) {
		QMutexLocker lock(&m->data_mutex);

		auto it = m->notfound.find(item.url);
		if (it == m->notfound.end()) {
			for (size_t i = 0; i < m->completed.size(); i++) {
				if (item.url == m->completed[i].url) {
					item = m->completed[i];
					m->completed.erase(m->completed.begin() + i);
					m->completed.insert(m->completed.begin(), item);
					return item.user;
				}
			}
			if (request) {
#if 0
				bool waiting = false;
				for (RequestItem const &r : m->requested) {
					if (item.url == r.url) {
						waiting = true;
						break;
					}
				}
				if (!waiting) {
					m->requested.push_back(item);
					m->condition.wakeOne();
				}
#else
				if (m->requested.empty()) {
					m->requested.push_back(item);
					m->condition.wakeOne();
				}
#endif
			}
		}
	}
	return GitHubAPI::User();
}

std::deque<CommitLoader::RequestItem> CommitLoader::takeResults()
{
	return std::move(m->completed);
}

void CommitLoader::interrupt()
{
	requestInterruption();
	if (m->web1) m->web1->close();
	if (m->web2) m->web2->close();
	m->condition.wakeAll();
}


