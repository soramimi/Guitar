#include "curlclient.h"
#include <curl/curl.h>
#include <vector>

// CurlContext

int CurlContext::instance_count_ = 0;

CurlContext::CurlContext()
{
	if (instance_count_ == 0) {
		curl_global_init(CURL_GLOBAL_DEFAULT);
	}
	instance_count_++;
}

CurlContext::~CurlContext()
{
	instance_count_--;
	if (instance_count_ == 0) {
		curl_global_cleanup();
	}
}

// CurlClient

struct CurlClient::Private {
	CURL *curl_ = nullptr;
	InetClient::Response response;
	InetClient::Error error_;
};

bool CurlClient::open()
{
	if (!m->curl_) {
		m->curl_ = curl_easy_init();
	}
	return (bool)m->curl_;
}

void CurlClient::close()
{
	if (m->curl_) {
		curl_easy_cleanup(m->curl_);
		m->curl_ = nullptr;
	}
}

void CurlClient::clear_error()
{
	m->error_ = {};
}

size_t CurlClient::write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t total_size = size * nmemb;
	InetClient::Response *res = static_cast<InetClient::Response *>(userp);
	char *data = static_cast<char *>(contents);
	res->content.insert(res->content.end(), data, data + total_size);
	return total_size;
}

CurlClient::CurlClient(CurlContext *cx)
	: m(new Private)
{
	(void)cx;
}

void CurlClient::reset()
{
	m->response = {};
	m->error_ = {};
}

CurlClient::~CurlClient()
{
	close();
	delete m;
}

int CurlClient::get(InetClient::Request const &req)
{
	clear_error();
	m->response.clear();  // 前回のデータをクリア

	if (!open()) {
		m->error_ = InetClient::Error("Failed to initialize CURL");
		m->response.code = 501;
		return m->response.code;
	}

	// Set URL
	char const *url = req.url().full_request().c_str();
	curl_easy_setopt(m->curl_, CURLOPT_URL, url);

	// Set callback function to receive data
	curl_easy_setopt(m->curl_, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(m->curl_, CURLOPT_WRITEDATA, &m->response);

	// Follow redirects
	curl_easy_setopt(m->curl_, CURLOPT_FOLLOWLOCATION, 1L);

	// Set timeout
	curl_easy_setopt(m->curl_, CURLOPT_TIMEOUT, 30L);

	// Perform the request
	CURLcode res = curl_easy_perform(m->curl_);

	if (res != CURLE_OK) {
		m->error_ = InetClient::Error(curl_easy_strerror(res));
		m->response.code = 501;
		return m->response.code;
	}

	long http_code = 0;
	curl_easy_getinfo(m->curl_, CURLINFO_RESPONSE_CODE, &http_code);
	m->response.code = http_code;
	return m->response.code;
}

int CurlClient::post(const InetClient::Request &req, const InetClient::Post *postdata)
{
	clear_error();
	m->response.clear();  // 前回のデータをクリア

	if (!open()) {
		m->error_ = InetClient::Error("Failed to initialize CURL");
		m->response.code = 501;
		return m->response.code;
	}
	// Set URL
	char const *url = req.url().full_request().c_str();
	curl_easy_setopt(m->curl_, CURLOPT_URL, url);

	// Set POST data
	curl_easy_setopt(m->curl_, CURLOPT_POST, 1L);
	if (postdata && !postdata->data.empty()) {
		curl_easy_setopt(m->curl_, CURLOPT_POSTFIELDS, postdata->data.data());
		curl_easy_setopt(m->curl_, CURLOPT_POSTFIELDSIZE, postdata->data.size());
	} else {
		curl_easy_setopt(m->curl_, CURLOPT_POSTFIELDS, "");
		curl_easy_setopt(m->curl_, CURLOPT_POSTFIELDSIZE, 0L);
	}

	// Set callback function to receive data
	curl_easy_setopt(m->curl_, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(m->curl_, CURLOPT_WRITEDATA, &m->response);
	// Follow redirects

	curl_easy_setopt(m->curl_, CURLOPT_FOLLOWLOCATION, 1L);
	// Set timeout
	curl_easy_setopt(m->curl_, CURLOPT_TIMEOUT, 30L);

	// Perform the request
	CURLcode res = curl_easy_perform(m->curl_);

	if (res != CURLE_OK) {
		m->error_ = InetClient::Error(curl_easy_strerror(res));
		m->response.code = 501;
		return m->response.code;
	}

	long http_code = 0;
	curl_easy_getinfo(m->curl_, CURLINFO_RESPONSE_CODE, &http_code);
	m->response.code = http_code;
	return m->response.code;
}

InetClient::Response const &CurlClient::response() const
{
	return m->response;
}

InetClient::Error const &CurlClient::error() const
{
	return m->error_;
}

size_t CurlClient::content_length() const
{
	return m->response.content.size();
}

const char *CurlClient::content_data() const
{
	if (m->response.content.empty()) return "";
	return m->response.content.data();
}




