#include "AvatarLoader.h"
#include "BasicMainWindow.h"
#include "MemoryReader.h"
#include "webclient.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QWaitCondition>

namespace {
const int MAX_CACHE_COUNT = 1000;
const int ICON_SIZE = 64;
}

using WebClientPtr = std::shared_ptr<WebClient>;

struct AvatarLoader::Private {
	QMutex mutex;
	QWaitCondition condition;
	std::deque<RequestItem> requested;
	std::deque<RequestItem> completed;
	BasicMainWindow *mainwindow = nullptr;
	WebClientPtr web;
};

AvatarLoader::AvatarLoader()
	: m(new Private)
{
}

AvatarLoader::~AvatarLoader()
{
	delete m;
}

void AvatarLoader::start(BasicMainWindow *mainwindow)
{
	m->mainwindow = mainwindow;
	QThread::start();
}

void AvatarLoader::run()
{
	m->web = std::make_shared<WebClient>(m->mainwindow->webContext());

	while (1) {
		if (isInterruptionRequested()) return;

		std::deque<RequestItem> requests;
		{
			QMutexLocker lock(&m->mutex);
			if (m->requested.empty()) {
				m->condition.wait(&m->mutex);
			}
			if (!m->requested.empty()) {
				std::swap(requests, m->requested);
			}
		}

		for (RequestItem &item : requests) {

			if (isInterruptionRequested()) return;

			if (item.email.empty()) continue;

			if (strchr(item.email.c_str(), '@')) {
				QString id;
				{
					QCryptographicHash hash(QCryptographicHash::Md5);
					hash.addData(item.email.c_str(), item.email.size());
					QByteArray ba = hash.result();
					char tmp[100];
					for (int i = 0; i < ba.size(); i++) {
						sprintf(tmp + i * 2, "%02x", ba.data()[i] & 0xff);
					}
					id = tmp;
				}
				QString url = "https://www.gravatar.com/avatar/%1?s=%2";
				url = url.arg(id).arg(ICON_SIZE);
				if (m->web->get(WebClient::URL(url.toStdString())) == 200) {
					if (!m->web->response().content.empty()) {
						MemoryReader reader(m->web->response().content.data(), m->web->response().content.size());
						reader.open(MemoryReader::ReadOnly);
						QImage image;
						image.load(&reader, nullptr);
						int w = image.width();
						int h = image.height();
						if (w > 0 && h > 0) {
							item.image = image;
							{
								QMutexLocker lock(&m->mutex);
								while (m->completed.size() >= MAX_CACHE_COUNT) {
									m->completed.pop_back();
								}
								m->completed.push_front(item);
							}
							emit updated();
							continue;
						}
					}
				} else {
					m->mainwindow->emitWriteLog(QString("Failed to fetch the avatar.\n").toUtf8());
					QString msg = QString::fromStdString(m->web->error().message() + '\n');
					m->mainwindow->emitWriteLog(msg.toUtf8());
				}
			}
		}
	}
}

QIcon AvatarLoader::fetch(std::string const &email, bool request) const
{
	QMutexLocker lock(&m->mutex);
	RequestItem item;
	item.email = email;
	for (size_t i = 0; i < m->completed.size(); i++) {
		if (item.email == m->completed[i].email) {
			item = m->completed[i];
			m->completed.erase(m->completed.begin() + i);
			m->completed.insert(m->completed.begin(), item);
			return QIcon(QPixmap::fromImage(item.image));
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
	return QIcon();
}

void AvatarLoader::stop()
{
	{
		QMutexLocker lock(&m->mutex);
		requestInterruption();
		m->requested.clear();
		m->condition.wakeAll();
	}
	if (!wait(3000)) {
		terminate();
	}
	if (m->web) {
		m->web->close();
		m->web.reset();
	}
	m->completed.clear();
}


