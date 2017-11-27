#include <windows.h>
#include "Win32Process.h"
#include <QThread>
#include <QTextCodec>
#include <deque>
#include <QDir>
#include <QDebug>


namespace {

class ReadThread : public QThread {
private:
	HANDLE hRead;
	QMutex *mutex;
	std::deque<char> *buffer;
protected:
	void run()
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
	ReadThread(HANDLE hRead, QMutex *mutex, std::deque<char> *buffer)
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
	QMutex mutex;
	std::string command;
	DWORD exit_code = -1;
	std::deque<char> input;
	std::deque<char> outvec;
	std::deque<char> errvec;
	bool use_input = false;
	HANDLE hInputWrite = INVALID_HANDLE_VALUE;
protected:
	void exec_command()
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
			sa.lpSecurityDescriptor = 0;
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

			// 子プロセスのエラー出力
			if (!DuplicateHandle(currproc, hErrorReadTmp, currproc, &hErrorRead, 0, TRUE, DUPLICATE_SAME_ACCESS))
				throw std::string("Failed to DuplicateHandle");

			// 子プロセスの標準出力
			if (!DuplicateHandle(currproc, hOutputReadTmp, currproc, &hOutputRead, 0, FALSE, DUPLICATE_SAME_ACCESS))
				throw std::string("Failed to DupliateHandle");

			// 不要なハンドルを閉じる
			CloseHandle(hOutputReadTmp);
			CloseHandle(hErrorReadTmp);
			CloseHandle(hInputWriteTmp);

			// プロセス起動
			PROCESS_INFORMATION pi;
			STARTUPINFOA si;

			ZeroMemory(&si, sizeof(STARTUPINFO));
			si.cb = sizeof(STARTUPINFO);
			si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
			si.hStdInput = hInputRead; // 標準入力ハンドル
			si.hStdOutput = hOutputWrite; // 標準出力ハンドル
			si.hStdError = hErrorWrite; // エラー出力ハンドル

			char *tmp = (char *)alloca(command.size() + 1);
			strcpy(tmp, command.c_str());
			if (!CreateProcessA(0, tmp, 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &si, &pi)) {
				throw std::string("Failed to CreateProcess");
			}

			// 不要なハンドルを閉じる
			CloseHandle(hOutputWrite);
			CloseHandle(hErrorWrite);
			CloseHandle(hInputRead);

			if (!use_input) {
				closeInput();
			}

			ReadThread t1(hOutputRead, &mutex, &outvec);
			ReadThread t2(hErrorRead, &mutex, &errvec);
			t1.start();
			t2.start();

			while (WaitForSingleObject(pi.hProcess, 1) != WAIT_OBJECT_0) {
				if (hInputWrite != INVALID_HANDLE_VALUE) {
					QMutexLocker lock(&mutex);
					int n = input.size();
					while (n > 0) {
						char tmp[1024];
						int l = n;
						if (l > sizeof(tmp)) {
							l = sizeof(tmp);
						}
						std::copy(input.begin(), input.begin() + l, tmp);
						input.erase(input.begin(), input.begin() + l);
						DWORD written;
						WriteFile(hInputWrite, tmp, l, &written, nullptr);
						n -= l;
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

	void run()
	{
		exec_command();
	}

	void closeInput()
	{
		CloseHandle(hInputWrite);
		hInputWrite = INVALID_HANDLE_VALUE;
	}

};

} // namespace

int Win32Process::run(QString const &command, bool use_input)
{
	QTextCodec *sjis = QTextCodec::codecForName("Shift_JIS");
	Q_ASSERT(sjis);

	QByteArray ba = sjis->fromUnicode(command);
	ba.push_back((char)0);
	char const *cmd = ba.data();

	Win32ProcessThread th;
	th.use_input = use_input;
	th.command = cmd;
	th.start();
	th.wait();

	outbytes.clear();
	errbytes.clear();
	outbytes.insert(outbytes.end(), th.outvec.begin(), th.outvec.end());
	errbytes.insert(errbytes.end(), th.errvec.begin(), th.errvec.end());
	return th.exit_code;
}

QString Win32Process::outstring() const
{
	return Process::toQString(outbytes);
}

QString Win32Process::errstring() const
{
	return Process::toQString(errbytes);
}


// experiment

struct Win32Process2::Private {
	QTextCodec *sjis = nullptr;
	Win32ProcessThread th;
	std::list<Win32Process2::Task> tasks;
};



Win32Process2::Win32Process2()
	: m(new Private)
{

}

Win32Process2::~Win32Process2()
{
	delete m;
}

void Win32Process2::start(bool use_input)
{
	m->th.use_input = use_input;

	m->sjis = QTextCodec::codecForName("Shift_JIS");
	Q_ASSERT(m->sjis);
}

void Win32Process2::exec(const QString &command)
{
	QByteArray ba = m->sjis->fromUnicode(command);
	if (ba.isEmpty()) return;
	Task task;
	char const *begin = ba.data();
	char const *end = begin + ba.size();
	task.command.assign(begin, end);
	m->tasks.push_back(task);
	step(false);
}

bool Win32Process2::step(bool delay)
{
	if (delay) {
		m->th.wait(1);
	}
	if (m->th.isRunning()) {
		return true;
	}
	if (!m->tasks.empty()) {
		Task task = m->tasks.front();
		m->tasks.pop_front();
		m->th.command = task.command;
		m->th.start();
		return true;
	}
	return false;
}

int Win32Process2::readOutput(char *dstptr, int maxlen)
{
	QMutexLocker lock(&m->th.mutex);
	int pos = 0;
	while (pos < maxlen) {
		int n;
		n = m->th.outvec.size();
		if (n > 0) {
			if (n > maxlen) {
				n = maxlen;
			}
			for (int i = 0; i < n; i++) {
				dstptr[pos++] = m->th.outvec.front();
				m->th.outvec.pop_front();

			}

		} else {
			n = m->th.errvec.size();
			if (n > 0) {
				if (n > maxlen) {
					n = maxlen;
				}
				for (int i = 0; i < n; i++) {
					dstptr[pos++] = m->th.errvec.front();
					m->th.errvec.pop_front();
				}
			} else {
				break;
			}
		}
	}
	return pos;
}

void Win32Process2::writeInput(const char *ptr, int len)
{
	QMutexLocker lock(&m->th.mutex);
	m->th.input.insert(m->th.input.end(), ptr, ptr + len);
}

void Win32Process2::closeInput()
{
	m->th.closeInput();
}

void Win32Process2::stop()
{
	if (m->th.isRunning()) {
		m->th.requestInterruption();
		closeInput();
		m->th.wait();
	}
}

