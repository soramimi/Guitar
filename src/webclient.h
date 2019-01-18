
#ifndef WEBCLIENT_H_
#define WEBCLIENT_H_

#include <vector>
#include <string>
#include <functional>

#define USE_OPENSSL 1
#define OPENSSL_NO_SHA1 1

class WebContext;
class WebClient;

#define CT_APPLICATION_X_WWW_FORM_URLENCODED "application/x-www-form-urlencoded"
#define CT_MULTIPART_FORM_DATA "multipart/form-data"

class WebClientHandler {
protected:
	void abort(std::string const &message = std::string());
public:
	virtual ~WebClientHandler() = default;
	virtual void checkHeader(WebClient * /*wc*/)
	{
	}
	virtual void checkContent(char const * /*ptr*/, size_t /*len*/)
	{
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

class WebClient {
public:
	class URL {
		friend class WebClient;
	private:
		struct Data {
			std::string full_request;
			std::string scheme;
			std::string host;
			int port = 0;
			std::string path;
		} data;
	public:
		URL() = default;
		URL(std::string const &addr);
		std::string const &scheme() const { return data.scheme; }
		std::string const &host() const { return data.host; }
		int port() const { return data.port; }
		std::string const &path() const { return data.path; }
		bool isssl() const;
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
		virtual ~Error() = default;
		std::string message() const
		{
			return msg_;
		}
	};
	struct Response {
		int code = 0;
		struct Version {
			unsigned int hi = 0;
			unsigned int lo = 0;
		} version;
		std::vector<std::string> header;
		std::vector<char> content;
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
		Part(char const *data, size_t size, std::string const &content_type = std::string())
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
	static int get_port(URL const *url, char const *scheme, char const *protocol);
	void set_default_header(URL const &url, Post const *post, const RequestOption &opt);
	std::string make_http_request(URL const &url, Post const *post, const WebProxy *proxy, bool https);
	void parse_http_header(char const *begin, char const *end, std::vector<std::string> *header);
	void parse_http_header(char const *begin, char const *end, Response *out);
	bool http_get(URL const &request_url, Post const *post, RequestOption const &opt, std::vector<char> *out);
	bool https_get(URL const &request_url, Post const *post, RequestOption const &opt, std::vector<char> *out);
	bool get(URL const &url, Post const *post, Response *out, WebClientHandler *handler);
	static void parse_header(std::vector<std::string> const *header, WebClient::Response *res);
	static std::string header_value(std::vector<std::string> const *header, std::string const &name);
	void append(char const *ptr, size_t len, std::vector<char> *out, WebClientHandler *handler);
	void on_end_header(const std::vector<char> *vec, WebClientHandler *handler);
	void receive_(const RequestOption &opt, std::function<int (char *, int)>, std::vector<char> *out);
	void output_debug_string(char const *str);
	void output_debug_strings(const std::vector<std::string> &vec);
	static void cleanup();
	void reset();
public:
	static void initialize();
	WebClient(WebContext *webcx);
	~WebClient();
	WebClient(WebClient const &) = delete;
	void operator = (WebClient const &) = delete;

	Error const &error() const;
	int get(URL const &url, WebClientHandler *handler = nullptr);
	int post(URL const &url, Post const *post, WebClientHandler *handler = nullptr);
	void close();
	void add_header(std::string const &text);
	Response const &response() const;
	std::string header_value(std::string const &name) const;
	std::string content_type() const;
	size_t content_length() const;
	char const *content_data() const;

	static void make_application_www_form_urlencoded(char const *begin, char const *end, WebClient::Post *out);
	static void make_multipart_form_data(const std::vector<Part> &parts, WebClient::Post *out, std::string const &boundary);
	static void make_multipart_form_data(char const *data, size_t size, WebClient::Post *out, std::string const &boundary);
};

class WebContext {
	friend class WebClient;
public:
private:
	struct Private;
	Private *m;
public:
	WebContext();
	~WebContext();
	WebContext(WebContext const &r) = delete;
	void operator = (WebContext const &r) = delete;

	void set_keep_alive_enabled(bool f);

	void set_http_proxy(std::string const &proxy);
	void set_https_proxy(std::string const &proxy);
	WebProxy const *http_proxy() const;
	WebProxy const *https_proxy() const;

	bool load_cacert(char const *path);
};

#endif
