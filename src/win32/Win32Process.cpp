#include "Win32Process.h"
#include <windows.h>
#include <QThread>
#include <QTextCodec>
#include <deque>

namespace {

class ReadThread : public QThread {
private:
	HANDLE hRead;
	std::deque<char> *buffer;
protected:
	void run()
	{
		while (1) {
			char buf[256];
			DWORD len = 0;
			if (!ReadFile(hRead, buf, sizeof(buf), &len, nullptr)) break;
			if (len < 1) break;
			if (buffer) buffer->insert(buffer->end(), buf, buf + len);
		}
	}
public:
	ReadThread(HANDLE hRead, std::deque<char> *buffer)
		: hRead(hRead)
		, buffer(buffer)
	{
	}
};

class Win32ProcessThread : public QThread {
private:
public:
	std::string command;
	DWORD exit_code = -1;
	std::deque<char> outvec;
	std::deque<char> errvec;
	AbstractProcess::stdinput_fn_t stdinput_callback_fn;
protected:
	void run()
	{
		outvec.clear();
		errvec.clear();
		try {
			HANDLE hInputWrite = INVALID_HANDLE_VALUE;
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

			if (!stdinput_callback_fn) {
				// 入力を閉じる
				CloseHandle(hInputWrite);
			}

			ReadThread t1(hOutputRead, &outvec);
			ReadThread t2(hErrorRead, &errvec);
			t1.start();
			t2.start();

			while (WaitForSingleObject(pi.hProcess, 1) != WAIT_OBJECT_0) {
				if (stdinput_callback_fn && !stdinput_callback_fn("", 0)) {
					break;
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
};

} // namespace

int Win32Process::run(QString const &command, stdinput_fn_t stdinput)
{
	QTextCodec *sjis = QTextCodec::codecForName("Shift_JIS");
	Q_ASSERT(sjis);

	QByteArray ba = sjis->fromUnicode(command);
	ba.push_back((char)0);
	char const *cmd = ba.data();

	Win32ProcessThread th;
	th.stdinput_callback_fn = stdinput;
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

