#include "GitHubAPI.h"

#include "webclient.h"
#include "misc.h"
#include "json.h"
#include "charvec.h"
#include "urlencode.h"
#include "MemoryReader.h"

#include <QDebug>

using WebClientPtr = GitHubAPI::WebClientPtr;

struct GitHubRequestThread::Private {
	WebContext webcx;
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

void GitHubRequestThread::run()
{
	ok = false;

	m->webcx.set_keep_alive_enabled(true);
	m->web = WebClientPtr(new WebClient(&m->webcx));

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
		th.start();
		while (!th.wait(1)) {
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
//		th.wait();
	}
	if (th.ok) {

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
	}

	std::sort(items.begin(), items.end(), [](SearchResultItem const &l, SearchResultItem const &r){
		return l.score > r.score; // 降順
	});

	return items;
}

QImage GitHubAPI::avatarImage(std::string const &name)
{
	QImage image;

	GitHubRequestThread th;
	{
		OverrideWaitCursor;
		th.url = "https://api.github.com/users/" + name;
		th.callback = [&](std::string const &text){
			std::string avatar_url;
			{
				JSON json;
				json.parse(text);
				for (JSON::Node const &node : json.node.children) {
					if (node.name == "avatar_url") {
						avatar_url = node.value;
					}
				}
			}
			if (!avatar_url.empty()) {
				WebContext webcx;
				WebClient web(&webcx);
				if (web.get(WebClient::URL(avatar_url)) == 200) {
					WebClient::Response const &r = web.response();
					if (!r.content.empty()) {
						MemoryReader reader(&r.content[0], r.content.size());
						reader.open(MemoryReader::ReadOnly);
						if (image.load(&reader, nullptr)) {
							return true;
						}
					}
				}
			}
			return false;
		};
		th.start();
		while (!th.wait(1)) {
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
//		th.wait();
	}

	return image;
}



