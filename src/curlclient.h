#ifndef CURLCLIENT_H
#define CURLCLIENT_H

#include "inetclient.h"
#include <string>
#include <vector>

class CurlContext {
private:
	static int instance_count_;
public:
	CurlContext();
	~CurlContext();
	CurlContext(CurlContext const &) = delete;
	void operator = (CurlContext const &) = delete;
};

class CurlClient : public AbstractInetClient {
public:
private:
	struct Private;
	struct Private *m;

	bool open();
	void clear_error();

	static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
public:
	CurlClient(CurlContext *cx);
	~CurlClient();
	CurlClient(CurlClient const &) = delete;
	void operator = (CurlClient const &) = delete;

	void reset() override;
	void close() override;
	InetClient::Error const &error() const override;

	int get(InetClient::Request const &req) override;

	InetClient::Response const &response() const override;

	size_t content_length() const override;

	char const *content_data() const override;
};

#endif // CURLCLIENT_H
