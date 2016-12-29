#include <QFileInfo>
#include <QtGlobal>
#ifdef Q_OS_WIN

#include "win32.h"
#include <Windows.h>
#include <ShlObj.h>

QString getModuleFileName()
{
	wchar_t tmp[300];
	DWORD n = GetModuleFileNameW(0, tmp, 300);
	return QString::fromUtf16((ushort const *)tmp, n);
}

QString getModuleFileDir()
{
	QString path = getModuleFileName();
	int i = path.lastIndexOf('\\');
	int j = path.lastIndexOf('/');
	if (i < j) i = j;
	if (i > 0) path = path.mid(0, i);
	return path;
}

QString getAppDataLocation()
{
	wchar_t tmp[300];
	if (SHGetSpecialFolderPathW(0, tmp, CSIDL_APPDATA, TRUE)) {
		return QString::fromUtf16((ushort const *)tmp);
	}
	return QString();
}


#include "thread.h"
#include "event.h"
#include <deque>
#define FAILED_(TEXT) throw std::string(TEXT)

class ProcessThread : Thread {
	friend class StreamThread;
private:
	Event start_event;
	QString command;
	HANDLE hOutputRead;
	HANDLE hInputWrite;
	HANDLE hErrorWrite;
	bool isProcessRunning = false;

	class StreamThread : public Thread {
	private:
		ProcessThread *procthread;
		std::deque<char> out;
		Mutex mutex;
	protected:
		virtual void run()
		{
			try {
				// 子プロセスの標準出力を読み出す
				while (1) {
					CHAR tmp[1024];
					DWORD len;
					if (!ReadFile(procthread->hOutputRead, tmp, sizeof(tmp), &len, 0) || len == 0) {
						if (GetLastError() == ERROR_BROKEN_PIPE) {
							break; // pipe done - normal exit path.
						}
						FAILED_("ReadFile"); // Something bad happened.
					}
					mutex.lock();
					out.insert(out.end(), tmp, tmp + len);
					mutex.unlock();
				}
			} catch (std::string const &e) { // 例外
				OutputDebugStringA(e.c_str());
			}
			procthread = 0;
		}
	public:
		void Prepare(ProcessThread *pt)
		{
			procthread = pt;
		}
		int ReadOutput(char *ptr, int len)
		{
			mutex.lock();
			size_t n = 0;
			if (ptr && len > 0) {
				n = out.size();
				if (len > len) {
					n = len;
				}
				for (size_t i = 0; i < n; i++) {
					*ptr++ = out.front();
					out.pop_front();
				}
			}
			mutex.unlock();
			if (n == 0 && !procthread) return -1;
			return n;
		}
	};
	StreamThread stream;

protected:

	void CloseOutput()
	{
		if (hOutputRead != INVALID_HANDLE_VALUE) {
			if (!CloseHandle(hOutputRead)) FAILED_("CloseHandle"); // 子プロセスの標準出力を閉じる
			hOutputRead = INVALID_HANDLE_VALUE;
		}
	}

	virtual void run()
	{
		hOutputRead = INVALID_HANDLE_VALUE;
		hInputWrite = INVALID_HANDLE_VALUE;
		hErrorWrite = INVALID_HANDLE_VALUE;

		HANDLE hOutputReadTmp = INVALID_HANDLE_VALUE;
		HANDLE hOutputWrite = INVALID_HANDLE_VALUE;
		HANDLE hInputWriteTmp = INVALID_HANDLE_VALUE;
		HANDLE hInputRead = INVALID_HANDLE_VALUE;

		try {
			SECURITY_ATTRIBUTES sa;

			sa.nLength = sizeof(SECURITY_ATTRIBUTES);
			sa.lpSecurityDescriptor = 0;
			sa.bInheritHandle = TRUE;

			HANDLE currproc = GetCurrentProcess();

			// パイプを作成
			if (!CreatePipe(&hOutputReadTmp, &hOutputWrite, &sa, 0))
				FAILED_("CreatePipe");

			// 子プロセスのエラー出力
			if (!DuplicateHandle(currproc, hOutputWrite, currproc, &hErrorWrite, 0, TRUE, DUPLICATE_SAME_ACCESS))
				FAILED_("DuplicateHandle");

			// パイプを作成
			if (!CreatePipe(&hInputRead, &hInputWriteTmp, &sa, 0))
				FAILED_("CreatePipe");

			// 子プロセスの標準出力
			if (!DuplicateHandle(currproc, hOutputReadTmp, currproc, &hOutputRead, 0, FALSE, DUPLICATE_SAME_ACCESS))
				FAILED_("DupliateHandle");

			// 子プロセスの標準入力
			if (!DuplicateHandle(currproc, hInputWriteTmp, currproc, &hInputWrite, 0, FALSE, DUPLICATE_SAME_ACCESS))
				FAILED_("DupliateHandle");

			// 不要なハンドルを閉じる
			CloseHandle(hOutputReadTmp);
			CloseHandle(hInputWriteTmp);

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

			std::vector<wchar_t> tmp;
			tmp.resize(command.size() + 1);
			wcscpy(&tmp[0], (wchar_t const *)command.utf16());
			if (!CreateProcessW(0, &tmp[0], 0, 0, TRUE, CREATE_NEW_CONSOLE, 0, 0, &si, &pi))
			//if (!CreateProcessA(0, &tmp[0], 0, 0, TRUE, CREATE_NEW_CONSOLE, 0, 0, &si, &pi))
				FAILED_("CreateProcess");

			// 不要なハンドルを閉じる
			CloseHandle(hOutputWrite);
			CloseHandle(hInputRead);
			CloseHandle(hErrorWrite);

			stream.start(); // ストリームスレッドを開始
			isProcessRunning = true;
			start_event.signal(); // 開始イベントを発行

			stream.join(); // ストリームスレッドの終了を待つ
			isProcessRunning = false;
			CloseOutput(); // 標準出力を閉じる

			//WaitForSingleObject(pi.hProcess, INFINITE); // プロセス終了を待つ

			// 終了
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		} catch (std::string const &e) { // 例外
			OutputDebugStringA(e.c_str());
		}
	}

	void WaitForStart() // プロセスの開始を待つ
	{
		start_event.wait();
	}

public:
	ProcessThread()
	{
	}

	~ProcessThread()
	{
		Join();
	}

	void Start(QString const &cmd, bool input)
	{
		command = cmd;
		stream.Prepare(this);
		start();
		WaitForStart();
		if (!input) {
			CloseInput();
		}
	}

	void WriteInput(char const *ptr, int len)
	{
		if (ptr && len > 0) {
			DWORD l = 0;
			WriteFile(hInputWrite, ptr, len, &l, 0);
		}
	}

	int ReadOutput(char *ptr, int len)
	{
		return stream.ReadOutput(ptr, len);
	}

	void CloseInput()
	{
		if (hInputWrite != INVALID_HANDLE_VALUE) {
			if (!CloseHandle(hInputWrite)) FAILED_("CloseHandle"); // 子プロセスの標準入力を閉じる
			hInputWrite = INVALID_HANDLE_VALUE;
		}
	}

	void Join()
	{
		CloseInput();
		join();
	}


	bool isRunning()
	{
		return isProcessRunning;
	}
};


QString hoge(QString const &s)
{
	std::vector<char> vec;
	vec.reserve(65536);
	ProcessThread proc;
	proc.Start(s, false);
	while (1) {
		char tmp[65536];
		bool r = proc.isRunning();
		int n = proc.ReadOutput(tmp, sizeof(tmp));
		if (n > 0) {
			vec.insert(vec.end(), tmp, tmp + n);
		} else if (!r) {
			break;
		}
		Sleep(0);
	}
	size_t n = vec.size();
	if (n > 0) {
		return QString::fromUtf8(&vec[0], n);
	}
	return QString();
}


#endif
