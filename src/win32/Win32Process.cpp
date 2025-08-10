#include <windows.h>
#include "Win32Process.h"
#include "TraceLogger.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMutex>
#include <QThread>
#include <deque>
#include <mutex>
#include <thread>

QString GetErrorMessage(DWORD e)
{
	QString msg;
	wchar_t *p = nullptr;
	size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, e, 0, (wchar_t *)&p, 0, nullptr);
	msg = QString::fromUtf16((ushort const *)p);
	LocalFree(p);
	return msg;
}

class OutputReaderThread {
private:
	HANDLE hRead;
	std::thread thread;
	std::mutex *mutex;
	std::deque<char> *buffer;
protected:
	void run()
	{
		char buf[4096];
		while (1) {
			DWORD len = 0;
			if (!ReadFile(hRead, buf, sizeof(buf), &len, nullptr)) break;
			if (len < 1) break;
			if (buffer) {
				std::lock_guard lock(*mutex);
				buffer->insert(buffer->end(), buf, buf + len);
			}
		}
	}
public:
	OutputReaderThread(HANDLE hRead, std::mutex *mutex, std::deque<char> *buffer)
		: hRead(hRead)
		, mutex(mutex)
		, buffer(buffer)
	{
	}
	~OutputReaderThread()
	{
		stop();
	}
	void start()
	{
		thread = std::thread([this](){
			run();
		});
	}
	void stop()
	{
		if (thread.joinable()) {
			thread.join();
		}
	}
	void wait()
	{
		stop();
	}
};

class Win32ProcessThread {
	friend class Win32Process2;
public:
	std::thread thread;
	std::mutex *mutex = nullptr;
	QString command;
	DWORD exit_code = -1;
	std::vector<char> input;
	std::deque<char> outq;
	std::deque<char> errq;
	bool use_input = false;
	HANDLE hInputWrite = INVALID_HANDLE_VALUE;
	bool close_input_later = false;

	// 環境変数をキャッシュして再利用
	static std::vector<wchar_t> cached_env;
	static std::mutex env_mutex;

	void reset()
	{
		mutex = nullptr;
		command.clear();
		exit_code = -1;
		input.clear();
		outq.clear();
		errq.clear();
		use_input = false;
		hInputWrite = INVALID_HANDLE_VALUE;
		close_input_later = false;
	}

protected:
	void run()
	{
		try {
			hInputWrite = INVALID_HANDLE_VALUE;
			HANDLE hOutputRead = INVALID_HANDLE_VALUE;
			HANDLE hErrorRead = INVALID_HANDLE_VALUE;

			HANDLE hInputRead = INVALID_HANDLE_VALUE;
			HANDLE hOutputWrite = INVALID_HANDLE_VALUE;
			HANDLE hErrorWrite = INVALID_HANDLE_VALUE;

			SECURITY_ATTRIBUTES sa;
			sa.nLength = sizeof(SECURITY_ATTRIBUTES);
			sa.lpSecurityDescriptor = nullptr;
			sa.bInheritHandle = TRUE;

			// パイプ作成の最適化
			const DWORD PIPE_BUFFER_SIZE = 65536;

			if (!CreatePipe(&hInputRead, &hInputWrite, &sa, PIPE_BUFFER_SIZE))
				throw std::string("Failed to CreatePipe");

			if (!CreatePipe(&hOutputRead, &hOutputWrite, &sa, PIPE_BUFFER_SIZE))
				throw std::string("Failed to CreatePipe");

			if (!CreatePipe(&hErrorRead, &hErrorWrite, &sa, PIPE_BUFFER_SIZE))
				throw std::string("Failed to CreatePipe");

			// ハンドルの継承可能性を最適化
			SetHandleInformation(hInputWrite, HANDLE_FLAG_INHERIT, 0);
			SetHandleInformation(hOutputRead, HANDLE_FLAG_INHERIT, 0);
			SetHandleInformation(hErrorRead, HANDLE_FLAG_INHERIT, 0);

			// プロセス起動
			PROCESS_INFORMATION pi;
			STARTUPINFOW si;

			ZeroMemory(&si, sizeof(STARTUPINFOW));
			si.cb = sizeof(STARTUPINFOW);
			si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
			si.hStdInput = hInputRead;
			si.hStdOutput = hOutputWrite;
			si.hStdError = hErrorWrite;

			TraceLogger trace;
			trace.begin("process", command);

			std::wstring wcmd(command.toStdWString());

			std::vector<wchar_t> *env_ptr = nullptr;
			{
				std::lock_guard<std::mutex> lock(env_mutex);
				if (cached_env.empty()) {
					wchar_t *p = GetEnvironmentStringsW();
					if (p) {
						int i = 0;
						while (p[i] || p[i + 1]) {
							i++;
						}
						cached_env.assign(p, p + i + 1);
						FreeEnvironmentStringsW(p);

						// LANG=en_US.UTF8を追加
						wchar_t const *e = L"LANG=en_US.UTF8";
						cached_env.insert(cached_env.end() - 1, e, e + wcslen(e) + 1);
					}
				}
				env_ptr = &cached_env;
			}

			// CreateProcessの最適化フラグ
			DWORD creation_flags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;

			if (!CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, TRUE, creation_flags, env_ptr->data(), nullptr, &si, &pi)) {
				DWORD e = GetLastError();
				qDebug() << e << GetErrorMessage(e);
				throw std::string("Failed to CreateProcess");
			}

			// 子プロセス側のハンドルをすぐに閉じる
			CloseHandle(hInputRead);
			CloseHandle(hOutputWrite);
			CloseHandle(hErrorWrite);

			if (!use_input) {
				closeInput();
			}

			OutputReaderThread t1(hOutputRead, mutex, &outq);
			OutputReaderThread t2(hErrorRead, mutex, &errq);
			t1.start();
			t2.start();

			HANDLE handles[] = {pi.hProcess};
			while (1) {
				DWORD wait_result = WaitForMultipleObjects(1, handles, FALSE, 10);
				if (wait_result == WAIT_OBJECT_0) break;
				if (wait_result == WAIT_FAILED) break;

				{
					std::lock_guard lock(*mutex);
					if (!input.empty()) {
						if (hInputWrite != INVALID_HANDLE_VALUE) {
							DWORD written;
							WriteFile(hInputWrite, input.data(), input.size(), &written, nullptr);
							input.clear();
						}
					} else if (close_input_later) {
						closeInput();
					}
				}
			}

			t1.wait();
			t2.wait();

			trace.end();

			CloseHandle(hOutputRead);
			CloseHandle(hErrorRead);

			GetExitCodeProcess(pi.hProcess, &exit_code);

			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		} catch (std::string const &e) {
			OutputDebugStringA(e.c_str());
		}
	}
public:
	Win32ProcessThread()
	{
	}
	~Win32ProcessThread()
	{
		stop();
	}
	void closeInput()
	{
		CloseHandle(hInputWrite);
		hInputWrite = INVALID_HANDLE_VALUE;
	}
	void writeInput(char const *ptr, int len)
	{
		std::lock_guard lock(*mutex);
		input.insert(input.end(), ptr, ptr + len);
	}
	void start()
	{
		thread = std::thread([this](){
			run();
		});
	}
	void stop()
	{
		if (thread.joinable()) {
			thread.join();
		}
	}
	void wait()
	{
		stop();
	}
};

// 静的メンバーの定義
std::vector<wchar_t> Win32ProcessThread::cached_env;
std::mutex Win32ProcessThread::env_mutex;

std::string toQString(const std::vector<char> &vec)
{
	if (vec.empty()) return {};
	return std::string(&vec[0], vec.size());
}

struct Win32Process::Private {
	std::mutex mutex;
	Win32ProcessThread th;
};

Win32Process::Win32Process()
	: m(new Private)
{
}

Win32Process::~Win32Process()
{
	delete m;
}

void Win32Process::start(std::string const &command, bool use_input)
{
	// 不要なメモリコピーを削減
	m->th.mutex = &m->mutex;
	m->th.use_input = use_input;
	m->th.command = QString::fromUtf8(command.c_str(), command.size());
	m->th.start();
}

int Win32Process::wait()
{
	m->th.wait();

	// move semanticsを使用してコピーを避ける
	outbytes.clear();
	errbytes.clear();
	outbytes.insert(outbytes.end(), m->th.outq.begin(), m->th.outq.end());
	errbytes.insert(errbytes.end(), m->th.errq.begin(), m->th.errq.end());
	int exit_code = m->th.exit_code;
	m->th.reset();
	return exit_code;
}

std::string Win32Process::outstring() const
{
	return toQString(outbytes);
}

std::string Win32Process::errstring() const
{
	return toQString(errbytes);
}

void Win32Process::writeInput(char const *ptr, int len)
{
	m->th.writeInput(ptr, len);
}

void Win32Process::closeInput(bool justnow)
{
	if (justnow) {
		m->th.closeInput();
	} else {
		m->th.close_input_later = true;
	}
}
