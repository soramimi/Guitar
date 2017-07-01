#ifndef GITHUBAPI_H
#define GITHUBAPI_H

#include <QImage>
#include <QThread>
#include <string>
#include <functional>
#include <memory>

class WebClient;


class GitHubAPI {
public:
	typedef std::shared_ptr<WebClient> WebClientPtr;

	struct SearchResultItem {
		std::string full_name;
		std::string description;
		std::string ssh_url;
		std::string clone_url;
		std::string html_url;
		double score = 0;
	};

	QList<GitHubAPI::SearchResultItem> searchRepository(const std::string &q);
	QImage avatarImage(const std::string &name);
};


class GitHubRequestThread : public QThread {
private:
	struct Private;
	Private *m;
protected:
	void run();
public:
	GitHubAPI::WebClientPtr web();
public:
	GitHubRequestThread();
	~GitHubRequestThread();
	std::string url;
	bool ok = false;
	std::string text;
	std::function<bool(std::string const &text)> callback;
};

#endif // GITHUBAPI_H
