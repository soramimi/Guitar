
#ifndef WEBCLIENT_H_
#define WEBCLIENT_H_

#include "inetclient.h"

#include <vector>
#include <string>
#include <functional>

#define USE_OPENSSL 1
#define OPENSSL_NO_SHA1 1

class WebContext;
class WebClient;

class WebClientHandler {
public:
	virtual ~WebClientHandler() = default;
	virtual void checkHeader(WebClient *wc)
	{
		(void)wc;
	}
	virtual void checkContent(char const *ptr, size_t len)
	{
		(void)ptr;
		(void)len;
	}
};

class WebProxy {
public:
	std::string server;
	bool empty() const
	{
		return server.empty();
	}
};

struct ResponseHeader;

class WebClient : public AbstractInetClient {
public:
	class ContentType {
	public:
		static constexpr const char *APPLICATION_OCTET_STREAM = "application/octet-stream";
		static constexpr const char *APPLICATION_X_WWW_FORM_URLENCODED = "application/x-www-form-urlencoded";
		static constexpr const char *MULTIPART_FORM_DATA = "multipart/form-data";
	};
	enum HttpVersion {
		HTTP_1_0,
		HTTP_1_1,
	};
	struct Post {
		std::string content_type;
		std::string boundary;
		std::vector<char> data;
	};
	struct ContentDisposition {
		std::string type;
		std::string name;
		std::string filename;
	};
	struct Part {
		char const *data = nullptr;
		size_t size = 0;
		std::string content_type;
		ContentDisposition content_disposition;
		std::string content_transfer_encoding;
		Part() = default;
		Part(char const *data, size_t size, std::string const &content_type = {})
			: data(data)
			, size(size)
			, content_type(content_type)
		{
		}
		void set_content_disposition(ContentDisposition const &cd)
		{
			content_disposition = cd;
		}
	};
	struct RequestOption {
		WebClientHandler *handler = nullptr;
		bool keep_alive = true;
	};
private:
	struct Private;
	Private *m;
	void clear_error();
	static int get_port(InetClient::URL const *url, char const *scheme, char const *protocol);
	void set_default_header(InetClient::Request const &url, Post const *post, const RequestOption &opt);
	std::string make_http_request(InetClient::Request const &url, Post const *post, const WebProxy *proxy, bool https);
	void parse_http_header(char const *begin, char const *end, std::vector<std::string> *header);
	void parse_http_header(char const *begin, char const *end, InetClient::Response *out);
	bool http_get(InetClient::Request const &request_req, Post const *post, RequestOption const &opt, ResponseHeader *rh, std::vector<char> *out);
	bool https_get(InetClient::Request const &request_url, Post const *post, RequestOption const &opt, ResponseHeader *rh, std::vector<char> *out);
	bool get(InetClient::Request const &req, Post const *post, InetClient::Response *out, WebClientHandler *handler);
	static void parse_header(std::vector<std::string> const *header, InetClient::Response *res);
	static std::string header_value(std::vector<std::string> const *header, std::string const &name);
	void append(char const *ptr, size_t len, std::vector<char> *out, WebClientHandler *handler);
	void on_end_header(const std::vector<char> *vec, WebClientHandler *handler);
	void receive_(const RequestOption &opt, std::function<int (char *, int)> const &, ResponseHeader *rh, std::vector<char> *out);
	void output_debug_string(char const *str);
	void output_debug_strings(const std::vector<std::string> &vec);
	static void cleanup();
protected:
	void reset() override;
public:
	static void initialize();
	WebClient(WebContext *webcx);
	~WebClient();
	WebClient(WebClient const &) = delete;
	void operator = (WebClient const &) = delete;

	void set_http_version(HttpVersion httpver);

	InetClient::Error const &error() const override;
	int get(InetClient::Request const &req, WebClientHandler *handler);
	int get(InetClient::Request const &req) override
	{
		return get(req, nullptr);
	}
	int post(InetClient::Request const &req, Post const *post, WebClientHandler *handler = nullptr);
	void close() override;
	void add_header(std::string const &text);
	InetClient::Response const &response() const override;
	std::string header_value(std::string const &name) const;
	std::string content_type() const;
	size_t content_length() const override;
	char const *content_data() const override;

	static void make_application_www_form_urlencoded(char const *begin, char const *end, WebClient::Post *out);
	static void make_multipart_form_data(const std::vector<Part> &parts, WebClient::Post *out, std::string const &boundary);
	static void make_multipart_form_data(char const *data, size_t size, WebClient::Post *out, std::string const &boundary);

	static std::string quick_get(const std::string &url);
	static std::string checkip();
};

class WebContext {
	friend class WebClient;
public:
private:
	struct Private;
	Private *m;
public:
	WebContext(WebClient::HttpVersion httpver);
	~WebContext();
	WebContext(WebContext const &r) = delete;
	void operator = (WebContext const &r) = delete;

	void set_http_version(WebClient::HttpVersion httpver);
	void set_keep_alive_enabled(bool f);

	void set_http_proxy(std::string const &proxy);
	void set_https_proxy(std::string const &proxy);
	WebProxy const *http_proxy() const;
	WebProxy const *https_proxy() const;

	bool load_cacert(char const *path);

	void notify_broken_pipe();
};

#endif
