
#include "GitHubAPI.h"
#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include "MemoryReader.h"
#include "OverrideWaitCursor.h"
#include "common/charvec.h"
#include "common/misc.h"
#include "urlencode.h"
#include "webclient.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>

using WebClientPtr = GitHubAPI::WebClientPtr;

struct GitHubRequestThread::Private {
	WebContext webcx = {WebClient::HTTP_1_0};
	WebClientPtr web;
};

GitHubRequestThread::GitHubRequestThread()
	: m(new Private)
{
}

GitHubRequestThread::~GitHubRequestThread()
{
	delete m;
}

void GitHubRequestThread::start()
{
	// m->mainwindow = mainwindow;
	m->webcx.set_keep_alive_enabled(false);
	m->web = std::make_shared<WebClient>(&m->webcx);
	QThread::start();
}

void GitHubRequestThread::run()
{
	ok = false;
	if (web()->get(WebClient::Request(url)) == 200) {
		WebClient::Response const &r = web()->response();
		if (!r.content.empty()) {
			text = to_stdstr(r.content);
			ok = true;
			if (callback) {
				ok = callback(text);
			}
		}
	} else {
		std::string msg = web()->error().message();
		if (!msg.empty()) {
			global->mainwindow->emitWriteLog(QString("Failed to access the site: %1\n").arg(QString::fromStdString(url)));
			global->mainwindow->emitWriteLog(QString("%1\n").arg(QString::fromStdString(msg)));
		}
	}
}

GitHubAPI::WebClientPtr GitHubRequestThread::web()
{
	return m->web;
}

QList<RepositorySearchResultItem> GitHubAPI::searchRepository(std::string q)
{
	q = url_encode(q);
	if (q.empty()) return {};

	QList<RepositorySearchResultItem> items;

	GitHubRequestThread th;
	{
		OverrideWaitCursor;
		th.url = "https://api.github.com/search/repositories?q=" + q;
		th.start();
		while (!th.wait(1)) {
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
	}
	if (th.ok) {
		QByteArray ba(th.text.c_str(), th.text.size());
		QJsonDocument doc = QJsonDocument::fromJson(ba);
		QJsonArray a1 = doc.object().value("items").toArray();
		for (QJsonValueRef const &v1 : a1) {
			QJsonObject o1 = v1.toObject();
			RepositorySearchResultItem item;
			auto String = [&](QString const &key){
				return o1.value(key).toString().toStdString();
			};
			item.full_name = String("full_name");
			if (!item.full_name.empty()) {
				item.description = String("description");
				item.html_url = String("html_url");
				item.ssh_url = String("ssh_url");
				item.clone_url = String("clone_url");
				item.score = o1.value("score").toDouble();
				items.push_back(item);
			}
		}
	}

	std::sort(items.begin(), items.end(), [](RepositorySearchResultItem const &l, RepositorySearchResultItem const &r){
		return l.score > r.score; // 降順
	});

	return items;
}



