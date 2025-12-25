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
	std::vector<char> response_data_;
	Error error_;
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
	m->error_ = Error();
}

size_t CurlClient::write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t total_size = size * nmemb;
	std::vector<char> *vec = static_cast<std::vector<char>*>(userp);
	char *data = static_cast<char*>(contents);
	vec->insert(vec->end(), data, data + total_size);
	return total_size;
}

CurlClient::CurlClient(CurlContext *cx)
	: m(new Private)
{
	(void)cx;
}

CurlClient::~CurlClient()
{
	close();
	delete m;
}

bool CurlClient::get(const Request &req)
{
	clear_error();
	m->response_data_.clear();  // 前回のデータをクリア

	if (!open()) {
		m->error_ = Error("Failed to initialize CURL");
		return false;
	}

	// Set URL
	char const *url = req.url().c_str();
	curl_easy_setopt(m->curl_, CURLOPT_URL, url);

	// Set callback function to receive data
	curl_easy_setopt(m->curl_, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(m->curl_, CURLOPT_WRITEDATA, &m->response_data_);

	// Follow redirects
	curl_easy_setopt(m->curl_, CURLOPT_FOLLOWLOCATION, 1L);

	// Set timeout
	curl_easy_setopt(m->curl_, CURLOPT_TIMEOUT, 30L);

	// Perform the request
	CURLcode res = curl_easy_perform(m->curl_);

	if (res != CURLE_OK) {
		m->error_ = Error(curl_easy_strerror(res));
		return false;
	}

	return true;
}

const CurlClient::Error &CurlClient::error() const
{
	return m->error_;
}

size_t CurlClient::content_length() const
{
	return m->response_data_.size();
}

const char *CurlClient::content_data() const
{
	if (m->response_data_.empty()) return "";
	return m->response_data_.data();
}
