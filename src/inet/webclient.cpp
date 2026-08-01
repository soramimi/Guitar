
#include "inetresolver.h"
#include <inet/webclient.h>
#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#pragma warning(disable:4996)
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#define closesocket(S) ::close(S)
using socket_t = int;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

#if USE_OPENSSL
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>
// #if (OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined SSL_get_peer_certificate)
// #if OPENSSL_VERSION_NUMBER >= 0x30000000L
// #define SSL_get_peer_certificate(s) SSL_get1_peer_certificate(s)
// #endif
#else
typedef void SSL;
typedef void SSL_CTX;
#endif

#include <set>
#include <cassert>
#include "base64.h"

#define USER_AGENT "Generic Web Client"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace {

constexpr size_t MAX_CHUNK_HEADER_VALUE = 10 * 1024 * 1024; // 10 MB
constexpr size_t MAX_CHUNK_TOTAL_SIZE   = 100 * 1024 * 1024; // 100 MB
constexpr size_t MAX_CHUNK_LINE_LENGTH  = 4096;

bool header_contains_newline(std::string const &s)
{
	return s.find('\r') != std::string::npos || s.find('\n') != std::string::npos;
}

void vappend(std::vector<char> *vec, char const *begin, char const *end)
{
	vec->insert(vec->end(), begin, end);
}

void vappend(std::vector<char> *vec, char const *p, size_t n)
{
	vappend(vec, p, p + n);
}

void vappend(std::vector<char> *vec, std::string_view const &s)
{
	vappend(vec, s.data(), s.size());
}

std::string_view trimmed(const std::string_view &s)
{
	size_t i = 0;
	size_t j = s.size();
	while (i < j && std::isspace((unsigned char)s[i])) i++;
	while (i < j && std::isspace((unsigned char)s[j - 1])) j--;
	return s.substr(i, j - i);
}

int x_stricmp(char const *s1, char const *s2)
{
#ifdef _WIN32
	return ::stricmp(s1, s2);
#else
	return ::strcasecmp(s1, s2);
#endif
}

int x_strnicmp(char const *s1, char const *s2, size_t n)
{
#ifdef _WIN32
	return ::strnicmp(s1, s2, n);
#else
	return ::strncasecmp(s1, s2, n);
#endif
}

} // namespace

struct WebContext::Private {
	WebClient::HttpVersion http_version = WebClient::HTTP_1_0;
	SSL_CTX *ctx = nullptr;
	bool use_keep_alive = false;
	WebProxy http_proxy;
	WebProxy https_proxy;
	bool broken_pipe = false;
	bool strict_certificate_verification = false; // 既存動作との互換性のためデフォルト無効
};

struct WebClient::Private {
	std::vector<std::string> request_header;
	InetClient::Error error;
	InetClient::Response response;
	WebContext *webcx;
	WebClient::HttpVersion http_version = WebClient::HTTP_1_0;
	int crlf_state = 0;
	size_t content_offset = 0;
	std::string last_host_name;
	int last_port = 0;
	bool keep_alive = false;
	socket_t sock = INVALID_SOCKET;
	SSL *ssl = nullptr;
};

WebClient::WebClient(WebContext *webcx)
	: m(new Private)
{
	assert(webcx);
	m->webcx = webcx;
	set_http_version(m->webcx->m->http_version);
}

WebClient::~WebClient()
{
	close();
	delete m;
}

void WebClient::set_http_version(HttpVersion httpver)
{
	m->http_version = httpver;
}

void WebClient::initialize()
{
#ifdef _WIN32
	static bool initialized = false;
	
	// Prevent multiple initialization
	if (!initialized) {
		WSADATA wsaData;
		WORD wVersionRequested;
		wVersionRequested = MAKEWORD(2, 2); // Request version 2.2 for better compatibility
		if (WSAStartup(wVersionRequested, &wsaData) == 0) {
			atexit(cleanup);
			initialized = true;
		}
	}
#endif

#if USE_OPENSSL
	// Thread-safe OpenSSL initialization
	static bool ssl_initialized = false;
	if (!ssl_initialized) {
		OpenSSL_add_all_algorithms();
		ssl_initialized = true;
	}
#endif
}

void WebClient::cleanup()
{
#if USE_OPENSSL
	ERR_free_strings();
#endif
#ifdef _WIN32
	WSACleanup();
#endif
}

void WebClient::reset()
{
	m->error = {};
	m->response = {};
	m->crlf_state = 0;
	m->content_offset = 0;
}

void WebClient::output_debug_string(char const *str)
{
	if (0) {
#ifdef _WIN32
		OutputDebugStringA(str);
#else
		fwrite(str, 1, strlen(str), stderr);
#endif
	}
}

void WebClient::output_debug_strings(std::vector<std::string> const &vec)
{
	for (std::string const &s : vec) {
		output_debug_string((s + '\n').c_str());
	}
}

InetClient::Error const &WebClient::error() const
{
	return m->error;
}

void WebClient::clear_error()
{
	m->error = {};
}

int WebClient::get_port(InetClient::URL const *url, char const *scheme, char const *protocol)
{
	int port = url->port();
	if (port < 1 || port > 65535) {
		struct servent *s;
		s = getservbyname(url->scheme().c_str(), protocol);
		if (s) {
			port = ntohs(s->s_port);
		} else {
			s = getservbyname(scheme, protocol);
			if (s) {
				port = ntohs(s->s_port);
			}
		}
		if (port < 1 || port > 65535) {
			port = 80;
		}
	}
	return port;
}

static inline std::string to_s(size_t n)
{
	char tmp[32]; // Sufficient for size_t on 64-bit systems
	snprintf(tmp, sizeof(tmp), "%zu", n); // Use %zu for size_t and prevent buffer overflow
	return tmp;
}

void WebClient::set_default_header(InetClient::Request const &url, bool is_standard_port, InetClient::Post const *postdata, RequestOption const &opt)
{
	std::vector<std::string> header;
	auto AddHeader = [&](std::string const &s){
		size_t i = s.find(':');
		if (i != std::string::npos) {
			std::string name = s.substr(0, i);
			auto it = std::find_if(header.begin(), header.end(), [&](std::string const &h){
					return h.size() > i && x_strnicmp(h.c_str(), name.c_str(), i) == 0 && h[i] == ':';
					});
			if (it != header.end()) {
				header.erase(it);
			}
			header.push_back(s);
		}
	};
	std::string host = url.url().host();
	if (!is_standard_port) {
		host += ':';
		host += std::to_string(url.url().port());
	}
	AddHeader("Host: " + host);
	AddHeader("User-Agent: " USER_AGENT);
	AddHeader("Accept: */*");
	if (opt.keep_alive) {
		AddHeader("Connection: keep-alive");
	} else {
		AddHeader("Connection: close");
	}
	if (postdata) {
		AddHeader("Content-Length: " + to_s(postdata->data.size()));
		std::string ct = "Content-Type: ";
		if (postdata->content_type.empty()) {
			ct += ContentType::APPLICATION_OCTET_STREAM;
		} else if (postdata->content_type == ContentType::MULTIPART_FORM_DATA) {
			ct += postdata->content_type;
			if (!postdata->boundary.empty()) {
				ct += "; boundary=";
				ct += postdata->boundary;
			}
		} else {
			ct += postdata->content_type;
		}
		AddHeader(ct);
	}
	if (url.auth().type == InetClient::Authorization::Basic) {
		std::string s = url.auth().uid + ':' + url.auth().pwd;
		AddHeader("Authorization: Basic " + base64_encode(s));
	}
	for (std::string const &h : url.headers()) {
		AddHeader(h);
	}
	m->request_header = std::move(header);
}

std::string WebClient::make_http_request(InetClient::Request const &url, InetClient::Post const *postdata, WebProxy const *proxy, bool https)
{
	std::string str;

	str = postdata ? "POST " : "GET ";

	char const *httpver = "1.0";
	switch (m->http_version) {
	case HTTP_1_1:
		httpver = "1.1";
		break;
	}

	if (proxy && !https) {
		str += url.url().full_request();
		str += " HTTP/";
		str += httpver;
		str += "\r\n";
	} else {
		str += url.url().path();
		str += " HTTP/";
		str += httpver;
		str += "\r\n";
	}

	for (std::string const &s: m->request_header) {
		str += s;
		str += "\r\n";
	}

	str += "\r\n";
	return str;
}

void WebClient::parse_http_header(char const *begin, char const *end, std::vector<std::string> *header)
{
	if (begin < end) {
		char const *left = begin;
		char const *right = left;
		while (1) {
			if (right >= end) {
				break;
			}
			if (*right == '\r' || *right == '\n') {
				if (left < right) {
					header->push_back(std::string(left, right));
				}
				if (right + 1 < end && *right == '\r' && right[1] == '\n') {
					right++;
				}
				right++;
				if (*right == '\r' || *right == '\n') {
					if (right + 1 < end && *right == '\r' && right[1] == '\n') {
						right++;
					}
					right++;
					break;
				}
				left = right;
			} else {
				right++;
			}
		}
	}
}

void WebClient::parse_http_header(char const *begin, char const *end, InetClient::Response *out)
{
	*out = {};
	parse_http_header(begin, end, &out->header);
	parse_header(&out->header, out);
}

static void send_(socket_t s, char const *ptr, int len)
{
	while (len > 0) {
		int n = std::min(len, 4096);
		n = send(s, ptr, n, 0);
		if (n < 1) {
			throw InetClient::Error("send request failed.", InetClient::Error::Network);
		}
		ptr += n;
		len -= n;
	}
}

void WebClient::on_end_header(std::vector<char> const *vec, WebClientHandler *handler)
{
	if (vec->empty()) return;
	char const *begin = &vec->at(0);
	char const *end = begin + vec->size();
	parse_http_header(begin, end, &m->response);
	if (handler) {
		handler->checkHeader(this);
	}
}

void WebClient::append(char const *ptr, size_t len, std::vector<char> *out, WebClientHandler *handler)
{
	size_t offset = out->size();
	out->insert(out->end(), ptr, ptr + len);

	if (m->crlf_state < 0) {
		// nop
	} else {
		for (size_t i = 0; i < len; i++) {
			int c = (unsigned char)ptr[i];
			if (c == '\r') {
				m->crlf_state |= 1;
			} else if (c == '\n') {
				m->crlf_state |= 1;
				m->crlf_state++;
			} else {
				m->crlf_state = 0;
			}
			if (m->crlf_state == 4) {
				m->content_offset = offset + i + 1;
				on_end_header(out, handler);
				m->crlf_state = -1;
				break;
			}
		}
	}
	if (handler && m->content_offset > 0) {
		offset = out->size();
		if (offset > m->content_offset) {
			size_t len = offset - m->content_offset;
			char const *ptr = &out->at(m->content_offset);
			handler->checkContent(ptr, len);
		}
	}
}

static char *stristr(char *str1, char const *str2)
{
	if (!str1 || !str2) return nullptr;
	
	size_t len1 = strlen(str1);
	size_t len2 = strlen(str2);
	
	if (len2 == 0) return str1; // Empty search string
	if (len2 > len1) return nullptr; // Search string longer than target
	
	for (size_t i = 0; i + len2 <= len1; i++) {
		if (x_strnicmp(str1 + i, str2, len2) == 0) {
			return str1 + i;
		}
	}
	return nullptr;
}

class ResponseHeader {
public:
	size_t pos = 0;
	std::vector<char> line;
	int content_length = -1;
	bool connection_keep_alive = false;
	bool connection_close = false;
	struct {
		bool chunked = false;
	} internal;
	int lf = 0;
	enum State {
		Header,
		Content,
	};
	State state = Header;
	void put(int c)
	{
		pos++;
		if (state == Header) {
			if (c== '\r' || c == '\n') {
				if (!line.empty()) {
					line.push_back(0);
					char *begin = &line[0];
					char *p = strchr(begin, ':');
					if (p && *p == ':') {
						*p++ = 0;
						auto IS = [&](char const *name){ return x_stricmp(begin, name) == 0; };
						if (IS("content-length")) {
							content_length = strtol(p, nullptr, 10);
						} else if (IS("connection")) {
							if (stristr(p, "keep-alive")) {
								connection_keep_alive = true;
							} else if (stristr(p, "close")) {
								connection_close = true;
							}
						} else if (IS("transfer-encoding")) {
							std::vector<std::string> vec;
							auto SPLIT = [](char const *str, char sep, std::vector<std::string> *out){
								out->clear();
								char const *begin = str;
								char const *end = begin + strlen(str);
								char const *ptr = begin;
								char const *left = ptr;
								while (1) {
									char c = 0;
									if (ptr < end) {
										c = *ptr;
									}
									if (c == sep || c == 0) {
										if (left < ptr) {
											char const *l = left;
											char const *r = ptr;
											while (l < r && isspace((unsigned char)*l)) l++;
											while (l < r && isspace((unsigned char)r[-1])) r--;
											out->emplace_back(l, r);
										}
										if (c == 0) break;
										ptr++;
										left = ptr;
									} else {
										ptr++;
									}
								}
							};
							SPLIT(p, ',', &vec);
							auto it = std::find(vec.begin(), vec.end(), "chunked");
							internal.chunked = it != vec.end();
						}
					}
					line.clear();
				}
				if (c== '\r') {
					return;
				}
				if (c == '\n') {
					lf++;
					if (lf == 2) {
						state = Content;
					}
					return;
				}
			}
			lf = 0;
			line.push_back(c);
		}
	}
};

void WebClient::receive_(RequestOption const &opt, std::function<int(char *, int)> const &rcv, ResponseHeader *rh, std::vector<char> *out)
{
	char buf[4096];
	size_t pos = 0;
	enum ChunkState {
		NO_CHUNK,
		CHUNK_HEADER,
		CHUNK_EXTENSION,
		CHUNK_DATA,
		CHUNK_END,
		CHUNK_TRAILER,
	};

	int chunk_state = NO_CHUNK;
	size_t chunked_offset = 0;
	size_t chunked_length = 0;
	size_t line_length = 0; // chunk size/extension 行の長さ
	size_t trailer_line_length = 0; // trailer 行の長さ
	while (1) {
		int n;
		if (rh->state == ResponseHeader::Content && chunk_state != NO_CHUNK) {
			if (chunked_length == 0) {
				std::string_view view(out->data() + chunked_offset, out->size() - chunked_offset);
				size_t i = 0;
				while (i < view.size()) {
					int c = (unsigned char)view[i++];
					if (chunk_state == CHUNK_HEADER) {
						if (c == '\n') {
							line_length = 0;
							if (chunked_length == 0) {
								chunk_state = CHUNK_TRAILER;
								continue;
							}
							chunk_state = CHUNK_DATA;
							continue;
						}
						if (c == '\r') continue;
						if (c == ';') {
							chunk_state = CHUNK_EXTENSION;
							continue;
						}
						if (!isxdigit(c)) {
							throw InetClient::Error("Invalid chunked encoding: unexpected character in chunk size");
						}
						if (++line_length > MAX_CHUNK_LINE_LENGTH) {
							throw InetClient::Error("Invalid chunked encoding: chunk size line too long");
						}
						// 16進変換（ロケール非依存）
						size_t digit;
						if (c >= '0' && c <= '9') {
							digit = c - '0';
						} else if (c >= 'a' && c <= 'f') {
							digit = c - 'a' + 10;
						} else {
							digit = c - 'A' + 10;
						}
						// オーバーフロー防止
						if (chunked_length > (std::numeric_limits<size_t>::max() - digit) / 16) {
							throw InetClient::Error("Invalid chunked encoding: chunk size overflow");
						}
						chunked_length = chunked_length * 16 + digit;
						if (chunked_length > MAX_CHUNK_HEADER_VALUE) {
							throw InetClient::Error("Invalid chunked encoding: chunk size exceeds limit");
						}
					} else if (chunk_state == CHUNK_EXTENSION) {
						if (c == '\n') {
							line_length = 0;
							if (chunked_length == 0) {
								chunk_state = CHUNK_TRAILER;
								continue;
							}
							chunk_state = CHUNK_DATA;
							continue;
						}
						if (c == '\r') continue;
						if (++line_length > MAX_CHUNK_LINE_LENGTH) {
							throw InetClient::Error("Invalid chunked encoding: chunk extension line too long");
						}
					} else if (chunk_state == CHUNK_DATA) {
						if (chunked_length > 0) {
							chunked_offset++;
							chunked_length--;
							if (chunked_length == 0) {
								chunk_state = CHUNK_END;
							}
						}
					} else if (chunk_state == CHUNK_END) {
						if (c == '\n') {
							chunk_state = CHUNK_HEADER;
							chunked_offset++;
							chunked_length = 0;
							continue;
						}
						if (c == '\r') {
							chunked_offset++;
							continue;
						}
						throw InetClient::Error("Invalid chunked encoding: expected CRLF after chunk data");
					} else if (chunk_state == CHUNK_TRAILER) {
						if (c == '\n') {
							if (trailer_line_length == 0) return;
							trailer_line_length = 0;
							continue;
						}
						if (c == '\r') continue;
						if (++trailer_line_length > MAX_CHUNK_LINE_LENGTH) {
							throw InetClient::Error("Invalid chunked encoding: trailer line too long");
						}
					}
				}
			}
			n = (int)sizeof(buf);
		} else if (rh->state == ResponseHeader::Content && rh->content_length >= 0) {
			n = int(rh->pos + rh->content_length - pos);
			n = std::min(n, (int)sizeof(buf));
			if (n < 1) break;
		} else {
			n = (int)sizeof(buf);
		}
		n = rcv(buf, n);
		if (n < 1) {
			if (chunk_state != NO_CHUNK) {
				throw InetClient::Error("Invalid chunked encoding: connection closed before end of chunks");
			}
			break;
		}
		append(buf, n, out, opt.handler);
		pos += n;
		if (rh->state == ResponseHeader::Header) {
			for (int i = 0; i < n; i++) {
				rh->put(buf[i]);
				if (rh->state == ResponseHeader::Content) {
					m->keep_alive = rh->connection_keep_alive && !rh->connection_close;
					if (rh->internal.chunked) {
						chunk_state = CHUNK_HEADER;
						chunked_offset = rh->pos;
					}
					break;
				}
			}
		}
	}
}

static socket_t inet_connect(std::string const &hostname, int port)
{
	socket_t sock = INVALID_SOCKET;
	InetResolver::Addr addr;

	std::mutex mutex;
	std::condition_variable cv;

	auto Check = [&](){
		std::lock_guard lock(mutex);
		return !addr; // Continue if addr is not yet set
	};

	auto Connect4 = [&](int delay){
		bool ret = false;
		if (Check()) {
			InetResolver::Addr addr4;
			if (InetResolver().resolve(hostname.data(), InetResolver::IN4, &addr4) && addr4) {
				if (delay > 0) {
					std::unique_lock lock(mutex);
					cv.wait_for(lock, std::chrono::milliseconds(delay));
				}
				if (Check()) {
					struct sockaddr_in host;
					memset((char *)&host, 0, sizeof(host));
					host.sin_family = AF_INET;
					host.sin_addr = *(in_addr const *)addr4.to_in4(0);
					host.sin_port = htons(port);
					socket_t sock4 = socket(AF_INET, SOCK_STREAM, 0);
					if (sock4 != INVALID_SOCKET) {
						if (Check()) {
							if (connect(sock4, (struct sockaddr *)&host, sizeof(host)) != SOCKET_ERROR) {
								std::lock_guard lock(mutex);
								if (!addr) {
									addr = addr4;
									sock = sock4;
									ret = true;
								}
							}
						}
						if (!ret) {
							closesocket(sock4);
						}
					}
				}
			}
		}
		cv.notify_all();
		return ret;
	};

	auto Connect6 = [&](int delay){
		bool ret = false;
		if (Check()) {
			InetResolver::Addr addr6;
			if (InetResolver().resolve(hostname.data(), InetResolver::IN6, &addr6) && addr6) {
				if (delay > 0) {
					std::unique_lock lock(mutex);
					cv.wait_for(lock, std::chrono::milliseconds(delay));
				}
				if (Check()) {
					struct sockaddr_in6 host;
					memset((char *)&host, 0, sizeof(host));
					host.sin6_family = AF_INET6;
					host.sin6_addr = *(in6_addr const *)addr6.to_in6(0);
					host.sin6_port = htons(port);
					socket_t sock6 = socket(AF_INET6, SOCK_STREAM, 0);
					if (sock6 != INVALID_SOCKET) {
						if (Check()) {
							if (connect(sock6, (struct sockaddr *)&host, sizeof(host)) != SOCKET_ERROR) {
								std::lock_guard lock(mutex);
								if (!addr) {
									addr = addr6;
									sock = sock6;
									ret = true;
								}
							}
						}
						if (!ret) {
							closesocket(sock6);
						}
					}
				}
			}
		}
		cv.notify_all();
		return ret;
	};

	std::thread thread6([&](){
		Connect6(0);
	});
	std::thread thread4([&](){
		Connect4(50);
	});
	{
		std::unique_lock lock(mutex);
		cv.wait(lock);
	}
	thread4.join();
	thread6.join();

	return sock;
}

bool WebClient::http_getpost(InetClient::Request const &request, InetClient::Post const *postdata, RequestOption const &opt, ResponseHeader *rh, std::vector<char> *out)
{
	clear_error();
	out->clear();

	InetClient::Request server_req;

	WebProxy const *proxy = m->webcx->http_proxy();
	if (proxy) {
		server_req = InetClient::Request(proxy->server);
	} else {
		server_req = request;
	}

	std::string hostname = server_req.url().host();
	int port = get_port(&server_req.url(), "http", "tcp");

	m->keep_alive = opt.keep_alive && hostname == m->last_host_name && port == m->last_port;
	if (!m->keep_alive) close();

	if (m->sock == INVALID_SOCKET) {
		m->sock = inet_connect(hostname, port);
		if (m->sock == INVALID_SOCKET) {
			throw InetClient::Error("connect failed.");
		}
	}
	m->last_host_name = hostname;
	m->last_port = port;

	constexpr int standard_port = 80;
	set_default_header(request, port == standard_port, postdata, opt);

	std::string req = make_http_request(request, postdata, proxy, false);

	bool ok = false;
	try {
		std::vector<char> sending;
		sending.reserve(req.size() + (postdata ? postdata->data.size() : 0));
		sending.insert(sending.end(), req.begin(), req.end());
		if (postdata && !postdata->data.empty()) {
			sending.insert(sending.end(), postdata->data.begin(), postdata->data.end());
		}
		// send_(m->sock, req.c_str(), (int)req.size());
		// if (postdata && !postdata->data.empty()) {
		// 	send_(m->sock, (char const *)&postdata->data[0], (int)postdata->data.size());
		// }
		send_(m->sock, sending.data(), (int)sending.size());
		ok = true;
	} catch (InetClient::Error const &e) {
		fprintf(stderr, "Send failed: %s\n", e.what().c_str());
	}

	m->crlf_state = 0;
	m->content_offset = 0;

	if (ok) {
		receive_(opt, [&](char *ptr, int len){
			return recv(m->sock, ptr, len, 0);
		}, rh, out);

		if (!m->keep_alive) close();
	}

	return ok;
}

bool WebClient::https_getpost(InetClient::Request const &request_req, InetClient::Post const *postdata, RequestOption const &opt, ResponseHeader *rh, std::vector<char> *out)
{
#if USE_OPENSSL

	auto *sslctx = m->webcx->m->ctx;
	if (!m->webcx || !sslctx) {
		output_debug_string("SSL context is null.\n");
		return false;
	}

	clear_error();
	out->clear();

	auto get_ssl_error = []()->std::string{
			char tmp[1000];
			unsigned long e = ERR_get_error();
			ERR_error_string_n(e, tmp, sizeof(tmp));
			return tmp;
			};

	InetClient::Request server_req;

	WebProxy const *proxy = m->webcx->https_proxy();
	if (proxy) {
		server_req = InetClient::Request(proxy->server);
	} else {
		server_req = request_req;
	}

	std::string hostname = server_req.url().host();
	int port = get_port(&server_req.url(), "https", "tcp");

	m->keep_alive = opt.keep_alive && hostname == m->last_host_name && port == m->last_port;
	if (!m->keep_alive) close();

	socket_t sock = m->sock;
	SSL *ssl = m->ssl;
	bool new_connection = false;
	
	if (sock == INVALID_SOCKET || !ssl) {
		new_connection = true;
		sock = inet_connect(hostname, port);
		if (sock == INVALID_SOCKET) {
			throw InetClient::Error("connect failed.");
		}
		ssl = nullptr; // Ensure ssl is nullptr before we try to create it
		
		try {
			if (proxy) { // Connect through proxy
				char port_str[16];
				snprintf(port_str, sizeof(port_str), ":%u", get_port(&request_req.url(), "https", "tcp"));

				std::string str = "CONNECT ";
				str += request_req.url().host();
				str += port_str;
				str += " HTTP/1.0\r\n\r\n";
				send_(sock, str.c_str(), (int)str.size());
				
				// Read proxy response
				char tmp[1000];
				int n = recv(sock, tmp, sizeof(tmp), 0);
				if (n <= 0) {
					throw InetClient::Error("Proxy connection failed");
				}
				
				// Parse response to check if connection succeeded
				bool found_ok = false;
				int i;
				for (i = 0; i < n - 8; i++) {
					if (strncmp(tmp + i, "200 OK", 6) == 0 ||
							strncmp(tmp + i, "200 Connection established", 26) == 0) {
						found_ok = true;
						break;
					}
				}
				
				if (!found_ok) {
					// Format response for error message
					int end = 0;
					for (i = 0; i < n && i < 100; i++) {
						if (tmp[i] == '\r' || tmp[i] == '\n') {
							end = i;
							break;
						}
					}
					throw InetClient::Error(std::string("Proxy error: ") + std::string(tmp, end));
				}
			}

			// Set up SSL
			ssl = SSL_new(sslctx);
			if (!ssl) {
				throw InetClient::Error(get_ssl_error());
			}

			// Disable insecure protocols
			SSL_set_options(ssl, SSL_OP_NO_SSLv2);
			SSL_set_options(ssl, SSL_OP_NO_SSLv3);
			
			// Set hostname for SNI and certificate verification
			SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
			if (!SSL_set1_host(ssl, hostname.c_str())) {
				throw InetClient::Error(get_ssl_error());
			}
			SSL_set_tlsext_host_name(ssl, hostname.c_str());

			int ret = SSL_set_fd(ssl, (int)sock);
			if (ret != 1) {
				throw InetClient::Error(get_ssl_error());
			}

			// Ensure PRNG is properly seeded
			RAND_poll();
			if (RAND_status() == 0) {
				// If RAND_poll didn't work, use a more secure method than rand()
				unsigned char rand_buf[32];
				for (int i = 0; i < 32; i++) {
					rand_buf[i] = (unsigned char)(time(NULL) ^ (i * 41) ^ (size_t)&ret);
				}
				RAND_add(rand_buf, sizeof(rand_buf), sizeof(rand_buf) / 4.0);
			}

			// Connect SSL
			ret = SSL_connect(ssl);
			if (ret != 1) {
				int err = SSL_get_error(ssl, ret);
				std::string error_msg = get_ssl_error();
				throw InetClient::Error("SSL connection failed: " + error_msg + " (code: " + std::to_string(err) + ")");
			}

			X509 *x509 = SSL_get_peer_certificate(ssl);
			if (!x509) {
				throw InetClient::Error("Server did not present a certificate");
			}
			
			// Verify certificate
			long verify_result = SSL_get_verify_result(ssl);
			if (verify_result != X509_V_OK) {
				std::string err = X509_verify_cert_error_string(verify_result);
				output_debug_string(("Certificate verification failed: " + err + "\n").c_str());
				if (m->webcx->m->strict_certificate_verification) {
					throw InetClient::Error("Certificate verification failed: " + err, InetClient::Error::Security);
				}
			}

			// Log certificate info in debug mode if needed
			if (0) {
				std::string cipher = SSL_get_cipher(ssl);
				output_debug_string((cipher + "\n").c_str());

				std::string version = SSL_get_cipher_version(ssl);
				output_debug_string((version + "\n").c_str());
			}
			
			X509_free(x509);
		} catch (...) {
			// Clean up resources on error
			if (ssl) {
				SSL_free(ssl);
				ssl = nullptr;
			}
			if (sock != INVALID_SOCKET) {
				closesocket(sock);
				sock = INVALID_SOCKET;
			}
			throw;
		}
	}
	
	// Update connection state
	m->last_host_name = hostname;
	m->last_port = port;

	// Prepare request
	constexpr int standard_port = 443;
	set_default_header(request_req, port == standard_port, postdata, opt);
	std::string request = make_http_request(request_req, postdata, proxy, true);

	// Send request
	auto SEND = [&](char const *ptr, int len){
		while (len > 0) {
			int n = SSL_write(ssl, ptr, len);
			if (n < 1 || n > len) {
				// Store error before potentially losing SSL context
				std::string error_msg = get_ssl_error();
				// We must clean up if this is a connection error
				if (new_connection) {
					if (ssl) {
						SSL_free(ssl);
						ssl = nullptr;
					}
					if (sock != INVALID_SOCKET) {
						closesocket(sock);
						sock = INVALID_SOCKET;
					}
				}
				throw InetClient::Error(error_msg);
			}
			ptr += n;
			len -= n;
		}
	};

	try {
		SEND(request.c_str(), (int)request.size());
		if (postdata && !postdata->data.empty()) {
			SEND((char const *)&postdata->data[0], (int)postdata->data.size());
		}

		m->crlf_state = 0;
		m->content_offset = 0;

		// Receive response
		receive_(opt, [&](char *ptr, int len){
			int n = SSL_read(ssl, ptr, len);
			if (n < 0) {
				// Store error info before cleanup
				std::string error_msg = get_ssl_error();
				if (new_connection) {
					// Clean up connection on error since it's not fully established
					if (ssl) {
						SSL_free(ssl);
						ssl = nullptr;
					}
					if (sock != INVALID_SOCKET) {
						closesocket(sock);
						sock = INVALID_SOCKET;
					}
				}
				throw InetClient::Error(error_msg);
			}
			return n;
		}, rh, out);

		// Save connection for reuse if keep-alive
		m->sock = sock;
		m->ssl = ssl;
		
		if (!m->keep_alive) {
			close();
		}
		return true;
	} catch (...) {
		// If there's an error during send/receive and this is a new connection,
		// we need to clean up to prevent resource leaks
		if (new_connection) {
			if (ssl) {
				SSL_free(ssl);
				m->ssl = nullptr;
			}
			if (sock != INVALID_SOCKET) {
				closesocket(sock);
				m->sock = INVALID_SOCKET;
			}
		}
		throw;
	}
#endif
	return false;
}

bool decode_chunked(char const *begin, char const *end, std::vector<char> *out)
{
	if (!out) return false;
	out->clear();
	if (!begin || begin > end) return false;
	if (begin == end) return true;

	char const *ptr = begin;
	size_t total_size = 0;

	auto parse_hex_digit = [](int c)->size_t {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		return c - 'A' + 10;
	};

	while (ptr < end) {
		// chunk size 行の終わりを探す
		char const *line_end = ptr;
		while (line_end < end && *line_end != '\n') {
			line_end++;
		}
		if (line_end >= end) {
			fprintf(stderr, "decode_chunked: truncated chunk size line\n");
			return false;
		}

		size_t line_len = static_cast<size_t>(line_end - ptr);
		if (line_len > MAX_CHUNK_LINE_LENGTH) {
			fprintf(stderr, "decode_chunked: chunk size line too long\n");
			return false;
		}

		// chunk size をパース（';' または '\r' まで）
		char const *p = ptr;
		size_t chunk_size = 0;
		while (p < line_end && *p != ';' && *p != '\r') {
			int c = (unsigned char)*p;
			if (!isxdigit(c)) {
				fprintf(stderr, "decode_chunked: invalid character in chunk size\n");
				return false;
			}
			size_t digit = parse_hex_digit(c);
			if (chunk_size > (std::numeric_limits<size_t>::max() - digit) / 16) {
				fprintf(stderr, "decode_chunked: chunk size overflow\n");
				return false;
			}
			chunk_size = chunk_size * 16 + digit;
			if (chunk_size > MAX_CHUNK_HEADER_VALUE) {
				fprintf(stderr, "decode_chunked: chunk size exceeds limit\n");
				return false;
			}
			p++;
		}

		// chunk-extension は line_end までスキップ（既に line_len で制限済み）
		ptr = line_end + 1;

		if (chunk_size == 0) {
			// 終端チャンク。trailer ヘッダーを空行までスキップ
			while (ptr < end) {
				char const *trailer_end = ptr;
				while (trailer_end < end && *trailer_end != '\n') {
					trailer_end++;
				}
				if (trailer_end >= end) {
					fprintf(stderr, "decode_chunked: truncated trailer\n");
					return false;
				}
				size_t trailer_line_len = static_cast<size_t>(trailer_end - ptr);
				if (trailer_line_len > MAX_CHUNK_LINE_LENGTH) {
					fprintf(stderr, "decode_chunked: trailer line too long\n");
					return false;
				}
				// 空行判定（"\r\n" または "\n"）
				if (trailer_line_len == 0 || (trailer_line_len == 1 && *ptr == '\r')) {
					return true;
				}
				ptr = trailer_end + 1;
			}
			fprintf(stderr, "decode_chunked: truncated trailer terminator\n");
			return false;
		}

		// chunk データ + 終端 CRLF が収まるか
		size_t remaining = static_cast<size_t>(end - ptr);
		if (remaining < chunk_size + 2) {
			fprintf(stderr, "decode_chunked: truncated chunk data\n");
			return false;
		}
		if (ptr[chunk_size] != '\r' || ptr[chunk_size + 1] != '\n') {
			fprintf(stderr, "decode_chunked: missing CRLF after chunk data\n");
			return false;
		}

		// 総サイズ上限
		if (total_size > MAX_CHUNK_TOTAL_SIZE - chunk_size) {
			fprintf(stderr, "decode_chunked: total size exceeds limit\n");
			return false;
		}
		total_size += chunk_size;

		out->insert(out->end(), ptr, ptr + chunk_size);
		ptr += chunk_size + 2;
	}

	fprintf(stderr, "decode_chunked: unexpected end of chunked data\n");
	return false;
}

bool WebClient::getpost(InetClient::Request const &req, InetClient::Post const *postdata, InetClient::Response *out, WebClientHandler *handler)
{
	reset();
	bool ok = false;
	try {
		if (!m->webcx->m) {
			throw InetClient::Error("WebContext is null.");
		}
		m->webcx->m->broken_pipe = false;
		RequestOption opt;
		opt.keep_alive = m->webcx->m->use_keep_alive;
		opt.handler = handler;
		ResponseHeader rh;
		std::vector<char> res;
		if (req.url().is_ssl()) {
#if USE_OPENSSL
			https_getpost(req, postdata, opt, &rh, &res);
#endif
		} else {
			http_getpost(req, postdata, opt, &rh, &res);
		}
		if (!res.empty()) {
			char const *begin = &res[0];
			char const *end = begin + res.size();
			char const *ptr = begin + m->content_offset;
			if (ptr < end) {
				if (rh.internal.chunked) {
					if (!decode_chunked(ptr, end, &out->content)) {
						out->content.clear();
						return false;
					}
				} else {
					out->content.assign(ptr, end);
				}
			}
		}
		ok = true;
	} catch (InetClient::Error const &e) {
		m->error = e;
		close();
	}
	if (m->webcx->m->broken_pipe) {
		m->webcx->m->broken_pipe = false;
		ok = false;
	}
	if (!ok) {
		*out = {};
	}
	return ok;
}

void WebClient::parse_header(std::vector<std::string> const *header, InetClient::Response *res)
{
	if (0) { // for debug
		for (std::string const &s : *header) {
			fprintf(stderr, "%s\n", s.c_str());
		}
	}
	if (!header->empty()) {
		std::string const &line = header->at(0);
		char const *begin = line.c_str();
		char const *end = begin + line.size();
		if (line.size() > 5 && strncmp(line.c_str(), "HTTP/", 5) == 0) {
			int state = 0;
			res->version.hi = res->version.lo = res->code = 0;
			char const *ptr = begin + 5;
			while (1) {
				int c = 0;
				if (ptr < end) {
					c = (unsigned char)*ptr;
				}
				switch (state) {
				case 0:
					if (isdigit(c)) {
						res->version.hi = res->version.hi * 10 + (c - '0');
					} else if (c == '.') {
						state = 1;
					} else {
						state = -1;
					}
					break;
				case 1:
					if (isdigit(c)) {
						res->version.lo = res->version.lo * 10 + (c - '0');
					} else if (isspace(c)) {
						state = 2;
					} else {
						state = -1;
					}
					break;
				case 2:
					if (isspace(c)) {
						if (res->code != 0) {
							state = -1;
						}
					} else if (isdigit(c)) {
						res->code = res->code * 10 + (c - '0');
					} else {
						state = -1;
					}
					break;
				default:
					state = -1;
					break;
				}
				if (c == 0 || state < 0) {
					break;
				}
				ptr++;
			}
		}
	}
}

int WebClient::get(const InetClient::Request &req, WebClientHandler *handler)
{
	if (getpost(req, nullptr, &m->response, handler)) {
		return m->response.code;
	}
	return -1;
}

int WebClient::post(const InetClient::Request &req, const InetClient::Post *postdata, WebClientHandler *handler)
{
	if (getpost(req, postdata, &m->response, handler)) {
		return m->response.code;
	}
	return -1;
}

int WebClient::get(const InetClient::Request &req)
{
	return get(req, nullptr);
}

int WebClient::post(const InetClient::Request &req, const InetClient::Post *postdata)
{
	return post(req, postdata, nullptr);
}

std::string WebClient::header_value(std::vector<std::string> const *header, std::string const &name)
{
	for (size_t i = 1; i < header->size(); i++) {
		std::string const &line = header->at(i);
		char const *begin = line.c_str();
		char const *end = begin + line.size();
		char const *colon = strchr(begin, ':');
		if (colon) {
			if (x_strnicmp(begin, name.c_str(), name.size()) == 0) {
				char const *ptr = colon + 1;
				while (ptr < end && isspace(*ptr & 0xff)) ptr++;
				return std::string(ptr, end);
			}
		}
	}
	return std::string();
}

std::string WebClient::header_value(std::string const &name) const
{
	return header_value(&m->response.header, name);
}

std::string WebClient::content_type() const
{
	std::string s = header_value("Content-Type");
	char const *begin = s.c_str();
	char const *end = begin + s.size();
	char const *ptr = begin;
	while (ptr < end) {
		int c = *ptr & 0xff;
		if (c == ';' || c < 0x21) break;
		ptr++;
	}
	if (ptr < end) return std::string(begin, ptr);
	return s;
}

size_t WebClient::content_length() const
{
	return m->response.content.size();
}

char const *WebClient::content_data() const
{
	if (m->response.content.empty()) return "";
	return &m->response.content[0];
}

void WebClient::close()
{
#if USE_OPENSSL
	if (m->ssl) {
		SSL_shutdown(m->ssl);
		SSL_free(m->ssl);
		m->ssl = nullptr;
	}
#endif
	if (m->sock != INVALID_SOCKET) {
		// Try graceful shutdown first
		int shutdown_result = shutdown(m->sock, 2); // SD_BOTH or SHUT_RDWR
		(void)shutdown_result; // Ignore shutdown errors as socket might already be disconnected
		
		closesocket(m->sock);
		m->sock = INVALID_SOCKET;
	}
	
	// Reset connection state
	m->last_host_name = "";
	m->last_port = 0;
	m->keep_alive = false;
}

void WebClient::add_header(std::string const &text)
{
	if (header_contains_newline(text)) {
		throw InetClient::Error("Invalid header: contains newline characters", InetClient::Error::Security);
	}
	m->request_header.push_back(text);
}

InetClient::Response const &WebClient::response() const
{
	return m->response;
}

void WebClient::make_application_www_form_urlencoded(char const *begin, char const *end, InetClient::Post *out)
{
	*out = InetClient::Post();
	out->content_type = ContentType::APPLICATION_X_WWW_FORM_URLENCODED;
	vappend(&out->data, begin, end - begin);
}

void WebClient::make_multipart_form_data(std::vector<Part> const &parts, InetClient::Post *out, std::string const &boundary)
{
	*out = InetClient::Post();
	out->content_type = ContentType::MULTIPART_FORM_DATA;
	out->boundary = boundary;

	for (Part const &part : parts) {
		vappend(&out->data, "--");
		vappend(&out->data, out->boundary);
		vappend(&out->data, "\r\n");
		if (!part.content_disposition.type.empty()) {
			ContentDisposition const &cd = part.content_disposition;
			std::string s;
			s = "Content-Disposition: ";
			s += cd.type;
			auto Add = [&s](std::string const &name, std::string const &value){
				if (!value.empty()) {
					s += "; " + name + "=\"";
					s += value;
					s += '\"';
				}
			};
			Add("name", cd.name);
			Add("filename", cd.filename);
			vappend(&out->data, s);
			vappend(&out->data, "\r\n");
		}
		if (!part.content_type.empty()) {
			vappend(&out->data, "Content-Type: " + part.content_type + "\r\n");
		}
		if (!part.content_transfer_encoding.empty()) {
			vappend(&out->data, "Content-Transfer-Encoding: " + part.content_transfer_encoding + "\r\n");
		}
		vappend(&out->data, "\r\n");
		vappend(&out->data, part.data, part.size);
		vappend(&out->data, "\r\n");
	}

	vappend(&out->data, "--");
	vappend(&out->data, out->boundary);
	vappend(&out->data, "--\r\n");
}

void WebClient::make_multipart_form_data(char const *data, size_t size, InetClient::Post *out, std::string const &boundary)
{
	Part part;
	part.data = data;
	part.size = size;
	std::vector<Part> parts;
	parts.push_back(part);
	make_multipart_form_data(parts, out, boundary);
}


//

WebContext::WebContext(WebClient::HttpVersion httpver)
	: m(new Private)
{
	set_http_version(httpver);
#if USE_OPENSSL
	SSL_load_error_strings();
	SSL_library_init();
	m->ctx = SSL_CTX_new(TLS_client_method());
	SSL_CTX_set_default_verify_paths(m->ctx);
	SSL_CTX_set_min_proto_version(m->ctx, TLS1_2_VERSION);
#endif
}

WebContext::~WebContext()
{
#if USE_OPENSSL
	SSL_CTX_free(m->ctx);
#endif
	delete m;
}

void WebContext::set_http_version(WebClient::HttpVersion httpver)
{
	m->http_version = httpver;
}

void WebContext::set_keep_alive_enabled(bool f)
{
	m->use_keep_alive = f;
}

void WebContext::set_http_proxy(std::string const &proxy)
{
	m->http_proxy = WebProxy();
	m->http_proxy.server = proxy;
}

void WebContext::set_https_proxy(std::string const &proxy)
{
	m->https_proxy = WebProxy();
	m->https_proxy.server = proxy;
}

const WebProxy *WebContext::http_proxy() const
{
	if (!m->http_proxy.empty()) {
		return &m->http_proxy;
	}
	return nullptr;
}

const WebProxy *WebContext::https_proxy() const
{
	if (!m->https_proxy.empty()) {
		return &m->https_proxy;
	}
	if (!m->http_proxy.empty()) {
		return &m->http_proxy;
	}
	return nullptr;
}

bool WebContext::load_cacert(char const *path)
{
#if USE_OPENSSL
	int r = SSL_CTX_load_verify_locations(m->ctx, path, nullptr);
	return r == 1;
#else
	return false;
#endif
}

void WebContext::set_strict_certificate_verification(bool strict)
{
	m->strict_certificate_verification = strict;
}

bool WebContext::is_strict_certificate_verification() const
{
	return m->strict_certificate_verification;
}

void WebContext::notify_broken_pipe()
{
	m->broken_pipe = true;
}

std::string WebClient::quick_get(std::string const &url)
{
	WebContext wc(WebClient::HTTP_1_1);
	wc.set_keep_alive_enabled(false);
	WebClient http(&wc);
	if (http.get(InetClient::Request(url))) {
		return {http.content_data(), http.content_length()};
	}
	return {};
}

std::string WebClient::checkip()
{
	auto s = quick_get("http://checkip.amazonaws.com/");
	return (std::string)trimmed(s);
}

#ifdef NDEBUG
#define ASSERT_REPORT(msg) do { fprintf(stderr, "%s\n", msg); std::abort(); } while (0)
#else
#define ASSERT_REPORT(msg) assert(false && (msg))
#endif

#define ASSERT_EQ(a, b) do { auto const &_a = (a); auto const &_b = (b); if (!(_a == _b)) ASSERT_REPORT("ASSERT_EQ failed: " #a " == " #b); } while (0)
#define ASSERT_NE(a, b) do { auto const &_a = (a); auto const &_b = (b); if (_a == _b) ASSERT_REPORT("ASSERT_NE failed: " #a " != " #b); } while (0)
#define ASSERT_TRUE(a) do { bool _v = (a); if (!_v) ASSERT_REPORT("ASSERT_TRUE failed: " #a); } while (0)
#define ASSERT_FALSE(a) do { bool _v = (a); if (_v) ASSERT_REPORT("ASSERT_FALSE failed: " #a); } while (0)

void WebClient::self_test()
{
	auto test_decode = [](char const *input)->std::string {
		std::vector<char> out;
		ASSERT_TRUE(decode_chunked(input, input + strlen(input), &out));
		return std::string(out.begin(), out.end());
	};

	// decode_chunked 正常系
	ASSERT_EQ(test_decode("5\r\nhello\r\n0\r\n\r\n"), "hello");
	ASSERT_EQ(test_decode("5\r\nhello\r\n5\r\nworld\r\n0\r\n\r\n"), "helloworld");
	ASSERT_EQ(test_decode("5;ext=value\r\nhello\r\n0\r\n\r\n"), "hello");
	ASSERT_EQ(test_decode("5\r\nhello\r\n0\r\nTrailer: value\r\n\r\n"), "hello");
	ASSERT_EQ(test_decode("0\r\n\r\n"), "");
	ASSERT_EQ(test_decode("A\r\n0123456789\r\n0\r\n\r\n"), "0123456789");
	ASSERT_EQ(test_decode("a\r\n0123456789\r\n0\r\n\r\n"), "0123456789");

	// decode_chunked 異常系
	{
		std::vector<char> out;
		std::string bad = "5\r\nhi\r\n0\r\n\r\n";
		ASSERT_FALSE(decode_chunked(bad.data(), bad.data() + bad.size(), &out));
	}
	{
		std::vector<char> out;
		std::string bad = "5\r\nhello\r\n";
		ASSERT_FALSE(decode_chunked(bad.data(), bad.data() + bad.size(), &out));
	}
	{
		std::vector<char> out;
		std::string bad = "zz\r\n\r\n";
		ASSERT_FALSE(decode_chunked(bad.data(), bad.data() + bad.size(), &out));
	}
	{
		std::vector<char> out;
		std::string bad = "5\r\nhello\r\n0\r\nTrailer\r\n";
		ASSERT_FALSE(decode_chunked(bad.data(), bad.data() + bad.size(), &out));
	}

	// ヘッダー改行検証
	{
		bool caught = false;
		try {
			InetClient::Request req("http://example.com/");
			req.add_header("X-Evil: value\r\nX-Injected: evil");
		} catch (InetClient::Error const &) {
			caught = true;
		}
		ASSERT_TRUE(caught);
	}
	{
		bool caught = false;
		try {
			WebContext wc(WebClient::HTTP_1_1);
			WebClient http(&wc);
			http.add_header("X-Evil: value\r\nX-Injected: evil");
		} catch (InetClient::Error const &) {
			caught = true;
		}
		ASSERT_TRUE(caught);
	}
}

