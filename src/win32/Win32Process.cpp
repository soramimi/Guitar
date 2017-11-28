#include <windows.h>
#include "Win32Process.h"
#include <QThread>
#include <QTextCodec>
#include <deque>
#include <QDir>
#include <QDebug>


namespace {

class OutputReaderThread : public QThread {
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

			OutputReaderThread t1(hOutputRead, &mutex, &outvec);
			OutputReaderThread t2(hErrorRead, &mutex, &errvec);
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

QString toQString(const std::vector<char> &vec)
{
	if (vec.empty()) return QString();
	return QString::fromUtf8(&vec[0], vec.size());
}

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
	return toQString(outbytes);
}

QString Win32Process::errstring() const
{
	return toQString(errbytes);
}


