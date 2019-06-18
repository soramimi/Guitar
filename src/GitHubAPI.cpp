
#include "GitHubAPI.h"
#include "BasicMainWindow.h"
#include "MemoryReader.h"
#include "charvec.h"
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
	BasicMainWindow *mainwindow = nullptr;
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

void GitHubRequestThread::start(BasicMainWindow *mainwindow)
{
	m->mainwindow = mainwindow;
	m->web = std::make_shared<WebClient>(m->mainwindow->webContext());
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
			m->mainwindow->emitWriteLog(QString::fromStdString("Failed to access the site: " + url + '\n').toUtf8());
			QString s = QString::fromStdString(msg + '\n');
			m->mainwindow->emitWriteLog(s.toUtf8());
		}
	}
}

GitHubAPI::WebClientPtr GitHubRequestThread::web()
{
	return m->web;
}

QList<GitHubAPI::SearchResultItem> GitHubAPI::searchRepository(std::string const &q)
{
	QList<GitHubAPI::SearchResultItem> items;

	GitHubRequestThread th;
	{
		OverrideWaitCursor;
		th.url = "https://api.github.com/search/repositories?q=" + q;
		th.start(mainwindow_);
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
			SearchResultItem item;
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

	std::sort(items.begin(), items.end(), [](SearchResultItem const &l, SearchResultItem const &r){
		return l.score > r.score; // 降順
	});

	return items;
}



