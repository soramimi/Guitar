#ifndef CURLCLIENT_H
#define CURLCLIENT_H

#include <string>

class CurlContext {
private:
	static int instance_count_;
public:
	CurlContext();
	~CurlContext();
	CurlContext(CurlContext const &) = delete;
	void operator = (CurlContext const &) = delete;
};

class CurlClient {
public:
	class Request {
	private:
		std::string url_;
	public:
		Request(std::string const &url)
			: url_(url)
		{
		}
		std::string const &url() const
		{
			return url_;
		}
	};
	class Error {
	private:
		std::string msg_;
	public:
		Error() = default;
		Error(std::string const &message)
			: msg_(message)
		{
		}
		std::string what() const
		{
			return msg_;
		}
	};
private:
	struct Private;
	struct Private *m;

	bool open();
	void close();
	void clear_error();

	static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
public:
	CurlClient(CurlContext *cx);
	~CurlClient();
	CurlClient(CurlClient const &) = delete;
	void operator = (CurlClient const &) = delete;

	bool get(Request const &req);

	Error const &error() const;

	size_t content_length() const;

	char const *content_data() const;
};

#endif // CURLCLIENT_H
