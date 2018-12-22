#ifndef GITHUBAPI_H
#define GITHUBAPI_H

#include <QImage>
#include <QThread>
#include <string>
#include <functional>
#include <memory>

class WebContext;
class WebClient;

class GitHubAPI {
public:
	using WebClientPtr = std::shared_ptr<WebClient>;

	struct User {
		std::string login;
		std::string avatar_url;
		std::string name;
		std::string email;
	};

	struct SearchResultItem {
		std::string full_name;
		std::string description;
		std::string ssh_url;
		std::string clone_url;
		std::string html_url;
		double score = 0;
	};

	WebContext *webcx;

	GitHubAPI(WebContext *webcx)
		: webcx(webcx)
	{
	}

	QList<GitHubAPI::SearchResultItem> searchRepository(const std::string &q);
};


class GitHubRequestThread : public QThread {
private:
	struct Private;
	Private *m;
protected:
	void run() override;
public:
	GitHubAPI::WebClientPtr web();
public:
	GitHubRequestThread();
	~GitHubRequestThread() override;
	std::string url;
	bool ok = false;
	std::string text;
	std::function<bool(std::string const &text)> callback;
	void start(WebContext *webcx);
};

#endif // GITHUBAPI_H
