
#include "Win32Util.h"

#include "ApplicationGlobal.h"
#include "common/joinpath.h"
#include "event.h"
#include "thread.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtGlobal>
#include <ShlObj.h>
#include <Windows.h>
#include <commoncontrols.h>
#include <deque>
#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>

#define FAILED_(TEXT) throw std::string(TEXT)

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QtWinExtras/QtWinExtras>

QPixmap pixmapFromHICON(HICON hIcon)
{
	return QtWin::fromHICON(hIcon);
}

#else

QPixmap pixmapFromHICON(HICON hIcon)
{
	return QPixmap::fromImage(QImage::fromHICON(hIcon));
}

#endif


std::wstring replace_slash_to_backslash(std::wstring const &str)
{
	std::wstring sb;
	for (size_t i = 0; i < str.size(); i++) {
		wchar_t c = str[i];
		if (c == '/') {
			c = '\\';
		}
		sb += c;
	}
	return sb;
}



QString getModuleFileName()
{
	wchar_t tmp[300];
	DWORD n = GetModuleFileNameW(nullptr, tmp, 300);
	return QString::fromUtf16((ushort const *)tmp, n);
}

QString getAppDataLocation()
{
	wchar_t tmp[300];
	if (SHGetSpecialFolderPathW(nullptr, tmp, CSIDL_APPDATA, TRUE)) {
		return QString::fromUtf16((ushort const *)tmp);
	}
	return QString();
}

class ProcessThread : public Thread {
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
		int ReadOutput(char *ptr, size_t len)
		{
			mutex.lock();
			size_t n = 0;
			if (ptr && len > 0) {
				n = out.size();
				if (n > len) {
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
	ProcessThread() = default;

	~ProcessThread() override
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
			QPixmap pm = pixmapFromHICON(shinfo.hIcon);
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



//

void createWin32Shortcut(Win32ShortcutData const &data)
{
	Win32ShortcutData d = data;

	d.lnkpath = replace_slash_to_backslash(d.lnkpath);

	HRESULT hr;
	IShellLink *shlink = 0;
	IShellLinkDataList *psldl = 0;
	IPersistFile *pfile = 0;

	hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID *)&shlink);
	if (SUCCEEDED(hr)) {
		shlink->SetPath(d.targetpath.c_str());
		if (!d.arguments.empty()) {
			shlink->SetArguments(d.arguments.c_str());
		}
		if (!d.workingdir.empty()) {
			shlink->SetWorkingDirectory(d.workingdir.c_str());
		}
		if (!d.iconpath.empty()) {
			shlink->SetIconLocation(d.iconpath.c_str(), d.iconindex);
		}

		if (d.runas) {
			hr = shlink->QueryInterface(IID_IShellLinkDataList, (void **)&psldl);
			if (SUCCEEDED(hr)) {
				psldl->SetFlags(SLDF_RUNAS_USER);
			}
		}

		hr = shlink->QueryInterface(IID_IPersistFile, (void**)&pfile);
		if (SUCCEEDED(hr)) {
			hr = pfile->Save(d.lnkpath.c_str(), TRUE);
			pfile->Release();
		}

		shlink->Release();
	}
}

namespace platform {

void createApplicationShortcut(QWidget *parent)
{
	QString desktop_dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
	QString target_path = QApplication::applicationFilePath();

	Win32ShortcutData data;
	data.targetpath = (wchar_t const *)target_path.utf16();

	QString launcher_dir = desktop_dir;
	QString name = APPLICATION_NAME;
	QString iconpath = target_path;//icon_dir / name + ".ico";
	QString launcher_path = launcher_dir / name + ".lnk";
	QString lnkpath = QFileDialog::getSaveFileName(parent, QApplication::tr("Save Launcher File"), launcher_path, "Launcher files (*.lnk)");
	data.iconpath = (wchar_t const *)iconpath.utf16();
	data.lnkpath = (wchar_t const *)lnkpath.utf16();

	if (!launcher_path.isEmpty()) {
		createWin32Shortcut(data);
	}
}

} // namespace platform

