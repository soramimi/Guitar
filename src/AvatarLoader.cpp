#include "AvatarLoader.h"
#include "MemoryReader.h"
#include "json.h"
#include "webclient.h"

#include <QCryptographicHash>
#include <QDebug>

namespace {
const int MAX_CACHE_COUNT = 1000;
const int ICON_SIZE = 64;
}

typedef std::shared_ptr<WebClient> WebClientPtr;

struct AvatarLoader::Private {
	QMutex data_mutex;
	QMutex thread_mutex;
	QWaitCondition condition;
	std::map<std::string, std::string> avatar_url_cache;
	std::deque<RequestItem> requested;
	std::deque<RequestItem> completed;
	std::set<std::string> notfound;
	WebContext webcx;
	WebClientPtr web1;
	WebClientPtr web2;
};

AvatarLoader::AvatarLoader()
	: m(new Private)
{
}

AvatarLoader::~AvatarLoader()
{
	delete m;
}

void AvatarLoader::run()
{
	m->webcx.set_keep_alive_enabled(true);
	m->web1 = WebClientPtr(new WebClient(&m->webcx));
	m->web2 = WebClientPtr(new WebClient(&m->webcx));

	while (1) {
		std::deque<RequestItem> requests;

		if (isInterruptionRequested()) break;

		m->thread_mutex.lock();
		{
			QMutexLocker lock(&m->data_mutex);
			requests = std::move(m->requested);
		}
		if (requests.empty()) {
			m->condition.wait(&m->thread_mutex);
			{
				QMutexLocker lock(&m->data_mutex);
				requests = std::move(m->requested);
			}
		}
		m->thread_mutex.unlock();

		for (RequestItem &item : requests) {

			if (isInterruptionRequested()) break;

			if (item.name.empty()) continue;

			if (strchr(item.name.c_str(), '@')) {
				QCryptographicHash hash(QCryptographicHash::Md5);
				hash.addData(item.name.c_str(), item.name.size());
				QByteArray ba = hash.result();
				char tmp[100];
				for (int i = 0; i < ba.size(); i++) {
					sprintf(tmp + i * 2, "%02x", ba.data()[i] & 0xff);
				}
				QString id = tmp;
				QString url = "https://www.gravatar.com/avatar/%1?s=%2";
				url = url.arg(id).arg(ICON_SIZE);
				if (m->web2->get(WebClient::URL(url.toStdString())) == 200) {
					if (!m->web2->response().content.empty()) {
						MemoryReader reader(m->web2->response().content.data(), m->web2->response().content.size());
						reader.open(MemoryReader::ReadOnly);
						QImage image;
						image.load(&reader, nullptr);
						int w = image.width();
						int h = image.height();
						if (w > 0 && h > 0) {
							item.icon = QIcon(QPixmap::fromImage(image));
							{
								QMutexLocker lock(&m->data_mutex);
								while (m->completed.size() >= MAX_CACHE_COUNT) {
									m->completed.pop_back();
								}
								m->completed.push_front(item);
							}
							emit updated();
							continue;
						}
					}
				}
			} else {
				std::string url = "https://api.github.com/users/" + item.name;

				std::string avatar_url;

				auto it = m->avatar_url_cache.find(url);
				if (it == m->avatar_url_cache.end()) {
					if (m->web1->get(url) == 200) {
						{
							JSON json;
							json.parse(&m->web1->response().content);
							for (JSON::Node const &node : json.node.children) {
								if (node.name == "avatar_url") {
									avatar_url = node.value;
									m->avatar_url_cache[url] = avatar_url;
								}
							}
						}
					}
				} else {
					avatar_url = it->second;
				}

				if (!avatar_url.empty()) {
					if (m->web2->get(avatar_url) == 200) {
						WebClient::Response const &r = m->web2->response();
						if (!r.content.empty()) {
							MemoryReader reader(&r.content[0], r.content.size());
							reader.open(MemoryReader::ReadOnly);
							QImage image;
							image.load(&reader, nullptr);
							int w = image.width();
							int h = image.height();
							if (w > 0 && h > 0) {
								if (w > ICON_SIZE) {
									h = ICON_SIZE * h / w;
									w = ICON_SIZE;
								} else if (h > ICON_SIZE) {
									w = ICON_SIZE * w / h;
									h = ICON_SIZE;
								}
								if (w < 1) w = 1;
								if (h < 1) h = 1;
								image = image.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
								item.icon = QIcon(QPixmap::fromImage(image));
								{
									QMutexLocker lock(&m->data_mutex);
									while (m->completed.size() >= MAX_CACHE_COUNT) {
										m->completed.pop_back();
									}
									m->completed.push_front(item);
								}
								emit updated();
								continue;
							}
						}
					}
				}
			}
			{ // not found
				QMutexLocker lock(&m->data_mutex);
				m->notfound.insert(m->notfound.end(), item.name);
			}
		}
	}
}

namespace {
bool isValidGitHubName(std::string const &name)
{
	size_t n = name.size();
	if (n < 1 || n > 39) return false;
	char const *begin = name.c_str();
	char const *end = begin + name.size();
	char const *ptr = begin;
	while (ptr < end) {
		int c = (unsigned char)*ptr;
		ptr++;
		if (isalnum(c)) {
			// ok
		} else if (c == '-') {
			if (ptr == begin) return false;
			if (ptr < end && *ptr == '-') return false;
		}
	}
	return true;
}

}

QIcon AvatarLoader::fetch(std::string const &name, bool request)
{
	RequestItem item;
	item.name = name;
	if (isValidGitHubName(item.name)) {
		QMutexLocker lock(&m->data_mutex);

		auto it = m->notfound.find(item.name);
		if (it == m->notfound.end()) {
			for (size_t i = 0; i < m->completed.size(); i++) {
				if (item.name == m->completed[i].name) {
					item = m->completed[i];
					m->completed.erase(m->completed.begin() + i);
					m->completed.insert(m->completed.begin(), item);
					return item.icon;
				}
			}
			if (request) {
				bool waiting = false;
				for (RequestItem const &r : m->requested) {
					if (item.name == r.name) {
						waiting = true;
						break;
					}
				}
				if (!waiting) {
					m->requested.push_back(item);
					m->condition.wakeOne();
				}
			}
		}
	}
	return QIcon();
}

void AvatarLoader::interrupt()
{
	requestInterruption();
	if (m->web1) m->web1->close();
	if (m->web2) m->web2->close();
	m->condition.wakeAll();
}


