#include <windows.h>
#include "Win32Process.h"
#include <QThread>
#include <QTextCodec>
#include <deque>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QMutex>

QString GetErrorMessage(DWORD e)
{
	QString msg;
	wchar_t *p = nullptr;
	size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, e, 0, (wchar_t *)&p, 0, nullptr);
	msg = QString::fromUtf16((ushort const *)p);
	LocalFree(p);
	return msg;
}

class OutputReaderThread : public QThread {
private:
	HANDLE hRead;
	QMutex *mutex;
	std::deque<char> *buffer;
protected:
	void run() override
	{
		while (1) {
			char buf[256];
			DWORD len = 0;
			if (!ReadFile(hRead, buf, sizeof(buf), &len, nullptr)) break;
			if (len < 1) break;
			if (buffer) {
				QMutexLocker lock(mutex);
				buffer->insert(buffer->end(), buf, buf + len);
			}
		}
	}
public:
	OutputReaderThread(HANDLE hRead, QMutex *mutex, std::deque<char> *buffer)
		: hRead(hRead)
		, mutex(mutex)
		, buffer(buffer)
	{
	}
};

class Win32ProcessThread : public QThread {
	friend class Win32Process2;
private:
public:
	QMutex *mutex = nullptr;
	QString command;
	DWORD exit_code = -1;
	std::deque<char> inq;
	std::deque<char> outq;
	std::deque<char> errq;
	bool use_input = false;
	HANDLE hInputWrite = INVALID_HANDLE_VALUE;
	bool close_input_later = false;

	void reset()
	{
		mutex = nullptr;
		command.clear();
		exit_code = -1;
		inq.clear();
		outq.clear();
		errq.clear();
		use_input = false;
		hInputWrite = INVALID_HANDLE_VALUE;
		close_input_later = false;
	}

protected:
	void run() override
	{
		try {
			hInputWrite = INVALID_HANDLE_VALUE;
			HANDLE hOutputRead = INVALID_HANDLE_VALUE;
			HANDLE hErrorRead = INVALID_HANDLE_VALUE;

			HANDLE hInputWriteTmp = INVALID_HANDLE_VALUE;
			HANDLE hOutputReadTmp = INVALID_HANDLE_VALUE;
			HANDLE hErrorReadTmp = INVALID_HANDLE_VALUE;
			HANDLE hInputRead = INVALID_HANDLE_VALUE;
			HANDLE hOutputWrite = INVALID_HANDLE_VALUE;
			HANDLE hErrorWrite = INVALID_HANDLE_VALUE;

			SECURITY_ATTRIBUTES sa;

			sa.nLength = sizeof(SECURITY_ATTRIBUTES);
			sa.lpSecurityDescriptor = nullptr;
			sa.bInheritHandle = TRUE;

			HANDLE currproc = GetCurrentProcess();

			// パイプを作成
			if (!CreatePipe(&hInputRead, &hInputWriteTmp, &sa, 0))
				throw std::string("Failed to CreatePipe");

			if (!CreatePipe(&hOutputReadTmp, &hOutputWrite, &sa, 0))
				throw std::string("Failed to CreatePipe");

			if (!CreatePipe(&hErrorReadTmp, &hErrorWrite, &sa, 0))
				throw std::string("Failed to CreatePipe");

			// 子プロセスの標準入力
			if (!DuplicateHandle(currproc, hInputWriteTmp, currproc, &hInputWrite, 0, FALSE, DUPLICATE_SAME_ACCESS))
				throw std::string("Failed to DupliateHandle");

			// 子プロセスの標準出力
			if (!DuplicateHandle(currproc, hOutputReadTmp, currproc, &hOutputRead, 0, FALSE, DUPLICATE_SAME_ACCESS))
				throw std::string("Failed to DupliateHandle");

			// 子プロセスのエラー出力
			if (!DuplicateHandle(currproc, hErrorReadTmp, currproc, &hErrorRead, 0, FALSE, DUPLICATE_SAME_ACCESS))
				throw std::string("Failed to DuplicateHandle");

			// 不要なハンドルを閉じる
			CloseHandle(hInputWriteTmp);
			CloseHandle(hOutputReadTmp);
			CloseHandle(hErrorReadTmp);

			// プロセス起動
			PROCESS_INFORMATION pi;
			STARTUPINFOW si;

			ZeroMemory(&si, sizeof(STARTUPINFO));
			si.cb = sizeof(STARTUPINFO);
			si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
			si.hStdInput = hInputRead; // 標準入力ハンドル
			si.hStdOutput = hOutputWrite; // 標準出力ハンドル
			si.hStdError = hErrorWrite; // エラー出力ハンドル

			int len = command.size();
			wchar_t *tmp = (wchar_t *)alloca(sizeof(wchar_t) * (len + 1));
			memcpy(tmp, command.utf16(), sizeof(wchar_t) * len);
			tmp[len] = 0;
			std::vector<wchar_t> env;
			{
#if 1
				wchar_t *p = GetEnvironmentStringsW();
				if (p) {
					int i = 0;
					while (p[i] || p[i + 1]) {
						i++;
					}
					env.insert(env.end(), p, p + i + 1);
					FreeEnvironmentStringsW(p);
				}
#endif
				wchar_t const *e = L"LANG=en_US.UTF8";
				env.insert(env.end(), e, e + wcslen(e) + 1);
				env.push_back(0);
			}
			if (!CreateProcessW(nullptr, tmp, nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, (void *)env.data(), nullptr, &si, &pi)) {
				DWORD e = GetLastError();
				qDebug() << e << GetErrorMessage(e);
				throw std::string("Failed to CreateProcess: ");
			}

			// 不要なハンドルを閉じる
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

			while (1) {
				auto r = WaitForSingleObject(pi.hProcess, 1);
				if (r == WAIT_OBJECT_0) break;
				if (r == WAIT_FAILED) break;
				{
					QMutexLocker lock(mutex);
					int n = inq.size();
					if (n > 0) {
						while (n > 0) {
							char tmp[1024];
							int l = n;
							if (l > sizeof(tmp)) {
								l = sizeof(tmp);
							}
							std::copy(inq.begin(), inq.begin() + l, tmp);
							inq.erase(inq.begin(), inq.begin() + l);
							if (hInputWrite != INVALID_HANDLE_VALUE) {
								DWORD written;
								WriteFile(hInputWrite, tmp, l, &written, nullptr);
							}
							n -= l;
						}
					} else if (close_input_later) {
						closeInput();
					}
				}
			}

			t1.wait();
			t2.wait();

			CloseHandle(hOutputRead);
			CloseHandle(hErrorRead);

			GetExitCodeProcess(pi.hProcess, &exit_code);

			// 終了
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		} catch (std::string const &e) { // 例外
			OutputDebugStringA(e.c_str());
		}
	}
public:
	void closeInput()
	{
		CloseHandle(hInputWrite);
		hInputWrite = INVALID_HANDLE_VALUE;
	}
	void writeInput(char const *ptr, int len)
	{
		QMutexLocker lock(mutex);
		inq.insert(inq.end(), ptr, ptr + len);
	}
};

QString toQString(const std::vector<char> &vec)
{
	if (vec.empty()) return QString();
	return QString::fromUtf8(&vec[0], vec.size());
}

struct Win32Process::Private {
	QMutex mutex;
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

void Win32Process::start(QString const &command, bool use_input)
{
	QByteArray ba = command.toUtf8();
	ba.push_back((char)0);
	char const *cmd = ba.data();

	m->th.mutex = &m->mutex;
	m->th.use_input = use_input;
	m->th.command = cmd;
	m->th.start();
}

int Win32Process::wait()
{
	m->th.wait();

	outbytes.clear();
	errbytes.clear();
	outbytes.insert(outbytes.end(), m->th.outq.begin(), m->th.outq.end());
	errbytes.insert(errbytes.end(), m->th.errq.begin(), m->th.errq.end());
	int exit_code = m->th.exit_code;
	m->th.reset();
	return exit_code;
}

QString Win32Process::outstring() const
{
	return toQString(outbytes);
}

QString Win32Process::errstring() const
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



