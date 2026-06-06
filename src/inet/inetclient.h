#ifndef INETCLIENT_H
#define INETCLIENT_H

#include <cassert>
#include <memory>
#include <string>
#include <vector>

class InetClient {
public:
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
	class URL {
	private:
		struct Data {
			std::string full_request;
			std::string scheme;
			std::string host;
			int port = 0;
			std::string path;
		} data_;
	public:
		URL() = default;
		URL(std::string const &addr);
		std::string const &full_request() const { return data_.full_request; }
		std::string const &scheme() const { return data_.scheme; }
		std::string const &host() const { return data_.host; }
		int port() const { return data_.port; }
		std::string const &path() const { return data_.path; }
		bool is_ssl() const;
	};
	struct Authorization {
		enum Type {
			None,
			Basic,
		} type = None;
		std::string uid;
		std::string pwd;
	};
	struct Post {
		std::string content_type;
		std::string boundary;
		std::vector<char> data;
	};
	class Request {
	private:
		InetClient::URL url_;
		Authorization auth_;
		std::vector<std::string> headers_;
	public:
		Request() = default;
		Request(std::string const &loc, std::vector<std::string> const &headers = {})
			: url_(loc)
			, headers_(headers)
		{
		}
		InetClient::URL const &url() const
		{
			return url_;
		}
		Authorization auth() const
		{
			return auth_;
		}
		std::vector<std::string> const &headers() const
		{
			return headers_;
		}
		void set_location(std::string const &loc)
		{
			url_ = InetClient::URL(loc);
		}
		void set_authorization(Authorization::Type type, std::string const &uid, std::string const &pwd)
		{
			auth_.type = type;
			auth_.uid = uid;
			auth_.pwd = pwd;
		}
		void set_basic_authorization(std::string const &uid, std::string const &pwd)
		{
			set_authorization(InetClient::Authorization::Basic, uid, pwd);
		}
		void add_header(std::string const &s)
		{
			headers_.push_back(s);
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
		bool empty() const
		{
			return content.empty();
		}
		void clear()
		{
			code = 0;
			content.clear();
		}
	};
};

class AbstractInetClient {
protected:
	virtual AbstractInetClient *clientptr()
	{
		return this;
	}
public:
	virtual ~AbstractInetClient() = default;
	AbstractInetClient *client()
	{
		return clientptr();
	}
	AbstractInetClient const *client() const
	{
		return const_cast<AbstractInetClient *>(this)->clientptr();
	}
	virtual void reset() = 0;
	virtual void close() = 0;
	virtual InetClient::Error const &error() const = 0;
	virtual int get(InetClient::Request const &req) = 0;
	virtual int post(InetClient::Request const &req, InetClient::Post const *post) = 0;
	virtual InetClient::Response const &response() const = 0;
	virtual size_t content_length() const = 0;
	virtual char const *content_data() const = 0;
};

#endif // INETCLIENT_H
