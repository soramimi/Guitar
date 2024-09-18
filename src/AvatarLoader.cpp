#include "AvatarLoader.h"
#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include "MemoryReader.h"
#include "webclient.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace {
const int MAX_CACHE_COUNT = 1000;
const int ICON_SIZE = 256;
}

using WebClientPtr = std::shared_ptr<WebClient>;

struct AvatarLoader::Private {
	volatile bool interrupted = false;
	std::mutex mutex;
	std::condition_variable condition;
	std::thread thread;
	std::deque<AvatarLoader::RequestItem> requests;
	MainWindow *mainwindow = nullptr;

	WebContext webcx = {WebClient::HTTP_1_1};
	WebClientPtr web;
};

AvatarLoader::AvatarLoader(QObject *parent)
	: QObject(parent)
	, m(new Private)
{
}

AvatarLoader::~AvatarLoader()
{
	delete m;
}

bool AvatarLoader::isInterruptionRequested() const
{
	return m->interrupted;
}

void AvatarLoader::requestInterruption()
{
	m->interrupted = true;
	m->condition.notify_all();
}

void AvatarLoader::run()
{
	m->web = std::make_shared<WebClient>(&m->webcx);

	while (1) {
		RequestItem request;
		{
			std::unique_lock<std::mutex> lock(m->mutex);
			if (isInterruptionRequested()) return;
			size_t i = m->requests.size();
			while (i > 0) {
				i--;
				RequestItem *req = &m->requests[i];
				if (req->state == Idle) {
					req->state = Busy;
					request = *req;
					break;
				}
			}
			if (request.state == Idle) {
				m->condition.wait(lock);
			}
			if (isInterruptionRequested()) return;
		}
		if (request.state == Busy) {
			if (misc::isValidMailAddress(request.email)) {
				QString id;
				{
					std::string email = request.email.trimmed().toLower().toStdString();
					QCryptographicHash hash(QCryptographicHash::Md5);
					hash.addData(email.c_str(), (int)email.size());
					QByteArray ba = hash.result();
					char tmp[100];
					for (int i = 0; i < ba.size(); i++) {
						sprintf(tmp + i * 2, "%02x", ba.data()[i] & 0xff);
					}
					id = tmp;
				}

				QStringList urls;
				auto const &provider = global->appsettings.avatar_provider;
				if (provider.gravatar)   urls.append(QString("https://www.gravatar.com/avatar/%1?s=%2&d=404").arg(id).arg(ICON_SIZE));
				if (provider.libravatar) urls.append(QString("https://www.libravatar.org/avatar/%1?s=%2&d=404").arg(id).arg(ICON_SIZE));

				auto getAvatar = [&](QString const &url)->std::optional<QImage>{
					if (m->web->get(WebClient::Request(url.toStdString())) == 200) {
						if (!m->web->response().content.empty()) {
							MemoryReader reader(m->web->response().content.data(), m->web->response().content.size());
							reader.open(MemoryReader::ReadOnly);
							QImage image;
							image.load(&reader, nullptr);
							int w = image.width();
							int h = image.height();
							if (w > 0 && h > 0) {
								return std::optional(image);
							}
						}
					}
					return std::nullopt;
				};

				QImage image;
				for (QString const &url : urls) {
					auto avatar = getAvatar(url);
					if (avatar) {
						image = avatar.value();
						break;
					}
				}

				if (image.width() > 0 && image.height() > 0) {
					request.image = image;
					request.state = Done;
				} else {
					QImage img(ICON_SIZE, ICON_SIZE, QImage::Format_ARGB32);
					img.fill(Qt::transparent);
					request.image = img;
					request.state = Fail;
				}
			}
			if (request.state == Busy) {
				request.state = Fail;
			}
			bool notify = false;
			{
				std::lock_guard<std::mutex> lock(m->mutex);
				size_t i = m->requests.size();
				while (i > 0) {
					i--;
					RequestItem *req = &m->requests[i];
					if (req->email == request.email) {
						if (request.state == Done || request.state == Fail) {
							*req = request;
							notify = true;
						} else {
							m->requests.erase(m->requests.begin() + i);
						}
						break;
					}
				}
			}
			if (notify) {
				emit ready();
			}
		}
	}
}

void AvatarLoader::start(MainWindow *mainwindow)
{
	m->mainwindow = mainwindow;
	m->interrupted = false;
	m->thread = std::thread([&](){
		run();
	});
}

void AvatarLoader::stop()
{
	requestInterruption();
	if (m->thread.joinable()) {
		m->thread.join();
	}
	m->requests.clear();
	if (m->web) {
		m->web->close();
		m->web.reset();
	}
}

QImage AvatarLoader::fetch(QString const &email, bool request) const
{
	if (misc::isValidMailAddress(email)) {
		std::lock_guard<std::mutex> lock(m->mutex);
		for (size_t i = 0; i < m->requests.size(); i++) {
			if (m->requests[i].email == email) {
				RequestItem item;
				if (m->requests[i].state == Done) {
					item = m->requests[i];
					m->requests.erase(m->requests.begin() + i);
					m->requests.insert(m->requests.begin(), item);
				}
				return item.image;
			}
		}
		if (request) {
			RequestItem item;
			item.state = Idle;
			item.email = email;
			{ // remove old requests
				size_t i = m->requests.size();
				while (i > 0 && m->requests.size() >= MAX_CACHE_COUNT) {
					i--;
					RequestItem *req = &m->requests[i];
					if (req->state == Done || req->state == Fail) {
						m->requests.erase(m->requests.begin() + i);
					}
				}
			}
			m->requests.push_back(item);
			m->condition.notify_all();
		}
	}
	return {};
}
