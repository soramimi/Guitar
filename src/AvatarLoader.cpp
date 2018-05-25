#include "AvatarLoader.h"
#include "MemoryReader.h"
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
	WebContext *webcx = nullptr;
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

void AvatarLoader::start(WebContext *webcx)
{
	m->webcx = webcx;
	QThread::start();
}

void AvatarLoader::run()
{
	m->web1 = WebClientPtr(new WebClient(m->webcx));
	m->web2 = WebClientPtr(new WebClient(m->webcx));

	while (1) {
		std::deque<RequestItem> requests;

		if (isInterruptionRequested()) return;

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

		if (isInterruptionRequested()) return;

		for (RequestItem &item : requests) {

			if (isInterruptionRequested()) return;

			if (item.email.empty()) continue;

			if (strchr(item.email.c_str(), '@')) {
				QCryptographicHash hash(QCryptographicHash::Md5);
				hash.addData(item.email.c_str(), item.email.size());
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
			}
			{ // not found
				QMutexLocker lock(&m->data_mutex);
				m->notfound.insert(m->notfound.end(), item.email);
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

QIcon AvatarLoader::fetch(std::string const &email, bool request)
{
	RequestItem item;
	item.email = email;
	if (isValidGitHubName(item.email)) {
		QMutexLocker lock(&m->data_mutex);

		auto it = m->notfound.find(item.email);
		if (it == m->notfound.end()) {
			for (size_t i = 0; i < m->completed.size(); i++) {
				if (item.email == m->completed[i].email) {
					item = m->completed[i];
					m->completed.erase(m->completed.begin() + i);
					m->completed.insert(m->completed.begin(), item);
					return item.icon;
				}
			}
			if (request) {
				bool waiting = false;
				for (RequestItem const &r : m->requested) {
					if (item.email == r.email) {
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


