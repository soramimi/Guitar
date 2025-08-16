#include "RemoteLogger.h"
#include "../src/common/htmlencode.h"
#include "../src/common/strformat.h"
#include <cstring>

#include "../src/webclient.h"

#ifdef _WIN32
#include <winsock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
typedef uint32_t pid_t;
#else
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <time.h>
#endif

struct RemoteLogger::Private {
	int sockfd = -1;
	struct sockaddr_in server_addr;
};

RemoteLogger::RemoteLogger()
	: m(new Private)
{
}

RemoteLogger::~RemoteLogger()
{
	close();
	delete m;
}

void RemoteLogger::initialize()
{
#ifdef _WIN32
	WSADATA wsaData;
	WORD wVersionRequested;
	wVersionRequested = MAKEWORD(2, 2); // Request version 2.2 for better compatibility
	if (WSAStartup(wVersionRequested, &wsaData) == 0) {
		atexit(cleanup);
	}
#endif
}

void RemoteLogger::cleanup()
{
#ifdef _WIN32
	WSACleanup();
#endif
}


bool RemoteLogger::open(char const *remote, int port)
{
	if (!remote) {
		remote = "127.0.0.1";
	}

	// ソケットの作成
	if ((m->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket creation failed");
		return false;
	}

	// サーバーアドレスの設定
	memset(&m->server_addr, 0, sizeof(m->server_addr));
	m->server_addr.sin_family = AF_INET;
	m->server_addr.sin_port = htons(port);

	HostNameResolver resolver;
	resolver.resolve(remote, &m->server_addr.sin_addr);
	// inet_pton(AF_INET, remote, &m->server_addr.sin_addr);
	return true;
}

void RemoteLogger::close()
{
	int fd = -1;
	std::swap(fd, m->sockfd);
	if (fd != -1) {
#ifdef _WIN32
		::closesocket(fd);
#else
		::close(fd);
#endif
	}
}


struct Timestamp {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int usec;
};

Timestamp getLocalTimeWithMicroseconds()
{
#ifdef _WIN32
	FILETIME ft;
	ULARGE_INTEGER li;

	// Windows 8 以降で高精度のシステム時刻を取得
	GetSystemTimePreciseAsFileTime(&ft);

	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;

	// UTC FILETIME（100ナノ秒単位） → ローカル FILETIME に変換
	FILETIME localFt;
	FileTimeToLocalFileTime(&ft, &localFt);

	// ローカル FILETIME → SYSTEMTIME に変換
	SYSTEMTIME st;
	FileTimeToSystemTime(&localFt, &st);

	// マイクロ秒を計算（100ナノ秒単位 → マイクロ秒）
	uint64_t totalMicroseconds = (static_cast<uint64_t>(ft.dwHighDateTime) << 32 | ft.dwLowDateTime) / 10;
	uint64_t microsecond = totalMicroseconds % 1000000;

	Timestamp ts;
	ts.year = st.wYear;
	ts.month = st.wMonth;
	ts.day = st.wDay;
	ts.hour = st.wHour;
	ts.minute = st.wMinute;
	ts.second = st.wSecond;
	ts.usec = microsecond;
	return ts;
#else
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	time_t now = tv.tv_sec;
	struct tm *lt = localtime(&now);
	Timestamp ts;
	ts.year = lt->tm_year + 1900;
	ts.month = lt->tm_mon + 1;
	ts.day = lt->tm_mday;
	ts.hour = lt->tm_hour;
	ts.minute = lt->tm_min;
	ts.second = lt->tm_sec;
	ts.usec = tv.tv_usec;
	return ts;
#endif
}


void RemoteLogger::send(std::string message, char const *file, int line)
{
#ifdef _WIN32
	pid_t pid = GetCurrentProcessId();
#else
	pid_t pid = getpid();
#endif
	Timestamp ts = getLocalTimeWithMicroseconds();

	message = strf("<pid>%d<d>%d-%02d-%02d<t>%02d:%02d:%02d<us>%06d<m>%s")
			(pid)
			(ts.year)(ts.month)(ts.day)
			(ts.hour)(ts.minute)(ts.second)
			(ts.usec)(html_encode(message));
	if (file && line > 0) {
		message += strf("<f>%s<l>%d")(html_encode(file))(line);
	}

	sendto(m->sockfd, message.c_str(), message.size(), 0, (const struct sockaddr *)&m->server_addr, sizeof(m->server_addr));
}
