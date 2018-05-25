#include "GitHubAPI.h"

#include "webclient.h"
#include "common/misc.h"
#include "charvec.h"
#include "urlencode.h"
#include "MemoryReader.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using WebClientPtr = GitHubAPI::WebClientPtr;

struct GitHubRequestThread::Private {
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

void GitHubRequestThread::start(WebContext *webcx)
{
	m->web = WebClientPtr(new WebClient(webcx));
	QThread::start();
}

void GitHubRequestThread::run()
{
	ok = false;
	if (web()->get(WebClient::URL(url)) == 200) {
		WebClient::Response const &r = web()->response();
		if (!r.content.empty()) {
			text = to_stdstr(r.content);
			ok = true;
			if (callback) {
				ok = callback(text);
			}
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
		th.start(webcx);
		while (!th.wait(1)) {
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
//		th.wait();
	}
	if (th.ok) {
#if 0
		JSON json;
		json.parse(th.text);
		for (JSON::Node const &node1 : json.node.children) {
			if (node1.name == "items") {
				for (JSON::Node const &node2 : node1.children) {
					SearchResultItem item;
					for (JSON::Node const &node3 : node2.children) {
						if (node3.name == "full_name") {
							item.full_name = node3.value;
						} else if (node3.name == "description") {
							item.description = node3.value;
						} else if (node3.name == "html_url") {
							item.html_url = node3.value;
						} else if (node3.name == "ssh_url") {
							item.ssh_url = node3.value;
						} else if (node3.name == "clone_url") {
							item.clone_url = node3.value;
						} else if (node3.name == "score") {
							item.score = strtod(node3.value.c_str(), nullptr);
						}
					}
					if (!item.full_name.empty()) {
						items.push_back(item);
					}
				}
			}
		}
#else
		QByteArray ba(th.text.c_str(), th.text.size());
		QJsonDocument doc = QJsonDocument::fromJson(ba);
		QJsonArray a1 = doc.object().value("items").toArray();
		for (QJsonValue const &v1 : a1) {
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
#endif
	}

	std::sort(items.begin(), items.end(), [](SearchResultItem const &l, SearchResultItem const &r){
		return l.score > r.score; // 降順
	});

	return items;
}



