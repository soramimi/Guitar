#include <QFileInfo>
#include <QtGlobal>

#include "win32.h"
#include <Windows.h>
#include <ShlObj.h>

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commoncontrols.h>
#include <QtWinExtras/QtWinExtras>


QString getModuleFileName()
{
	wchar_t tmp[300];
	DWORD n = GetModuleFileNameW(nullptr, tmp, 300);
	return QString::fromUtf16((ushort const *)tmp, n);
}

//QString getModuleFileDir()
//{
//	QString path = getModuleFileName();
//	int i = path.lastIndexOf('\\');
//	int j = path.lastIndexOf('/');
//	if (i < j) i = j;
//	if (i > 0) path = path.mid(0, i);
//	return path;
//}

QString getAppDataLocation()
{
	wchar_t tmp[300];
	if (SHGetSpecialFolderPathW(nullptr, tmp, CSIDL_APPDATA, TRUE)) {
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
	DWORD exit_code = 0;
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
		 void run() override
		{
			try {
				// 子プロセスの標準出力を読み出す
				while (1) {
					CHAR tmp[1024];
					DWORD len;
					if (!ReadFile(procthread->hOutputRead, tmp, sizeof(tmp), &len, nullptr) || len == 0) {
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
			procthread = nullptr;
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

	 void run() override
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
			sa.lpSecurityDescriptor = nullptr;
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
			if (!CreateProcessW(nullptr, &tmp[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
				FAILED_("CreateProcess");
			}

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

			WaitForSingleObject(pi.hProcess, INFINITE); // プロセス終了を待つ
			GetExitCodeProcess(pi.hProcess, &exit_code);

			// 終了
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		} catch (std::string const &e) { // 例外
			stream.stop();
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
		WaitForExit();
	}

	void Start(QString const &cmd, bool input)
	{
		command = cmd;
		exit_code = 0;
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
			WriteFile(hInputWrite, ptr, len, &l, nullptr);
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

	int WaitForExit()
	{
		CloseInput();
		join();
		return exit_code;
	}

	bool IsRunning()
	{
		return isProcessRunning;
	}
};

int winRunCommand(QString const &cmd, QByteArray *out)
{
	out->clear();
	ProcessThread proc;
	proc.Start(cmd, false);
	while (1) {
		char tmp[1024];
		bool r = proc.IsRunning();
		int n = proc.ReadOutput(tmp, sizeof(tmp));
		if (n > 0) {
			out->append(tmp, n);
		} else if (!r) {
			break;
		}
		Sleep(0);
	}
	return proc.WaitForExit();
}


void setEnvironmentVariable(QString const &name, QString const &value)
{
	SetEnvironmentVariableW((wchar_t const *)name.utf16(), (wchar_t const *)value.utf16());
}


QString getWin32HttpProxy()
{
	HKEY hk = nullptr;
	auto Close = [&](){
		RegCloseKey(hk);
	};
	try {
		LSTATUS s;
		s = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 0, KEY_READ, &hk);
		if (s != ERROR_SUCCESS) throw s;
		char tmp[1000];
		DWORD type = 0;
		DWORD len = sizeof(tmp);
		s = RegQueryValueExA(hk, "ProxyServer", nullptr, &type, (unsigned char *)tmp, &len);
		if (s != ERROR_SUCCESS) throw s;
		while (len > 0 && tmp[len - 1] == 0) {
			len--;
		}
		Close();
		return QString::fromLatin1(tmp, len);
	} catch (LSTATUS s) {
		Close();
		(void)s;
	}
	return QString();
}




namespace {
QIcon iconFromExtension_(QString const &ext, UINT flag)
{
	QIcon icon;
	QString name = "*." + ext;
	SHFILEINFOW shinfo;
	if (SHGetFileInfoW((wchar_t const *)name.utf16(), 0, &shinfo, sizeof(shinfo), flag | SHGFI_ICON | SHGFI_USEFILEATTRIBUTES) != 0) {
		if (shinfo.hIcon) {
			QPixmap pm = QtWin::fromHICON(shinfo.hIcon);
			if (!pm.isNull()) {
				icon = QIcon(pm);
			}
			DestroyIcon(shinfo.hIcon);
		}
	}
	return icon;
}
}

QIcon winIconFromExtensionLarge(QString const &ext)
{
	return iconFromExtension_(ext, SHGFI_LARGEICON);
}

QIcon winIconFromExtensionSmall(QString const &ext)
{
	return iconFromExtension_(ext, SHGFI_SMALLICON);
}
