#include "Win32Process.h"
#include <windows.h>
#include <QThread>
#include <QTextCodec>

namespace {
class ReadThread : public QThread {
private:
	HANDLE hRead;
	std::vector<char> *buffer;
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
	ReadThread(HANDLE hRead, std::vector<char> *buffer)
		: hRead(hRead)
		, buffer(buffer)
	{
	}
};
}

uint32_t Win32Process::run(const std::string &command)
{
	DWORD exit_code = -1;
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

		// 入力を閉じる
		CloseHandle(hInputWrite);

		ReadThread t1(hOutputRead, &outvec);
		ReadThread t2(hErrorRead, &errvec);
		t1.start();
		t2.start();

		WaitForSingleObject(pi.hProcess, INFINITE);

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
	return exit_code;
}

std::string Win32Process::outstring() const
{
	if (outvec.empty()) {
		return std::string();
	} else {
		return std::string(&outvec[0], outvec.size());
	}
}

std::string Win32Process::errstring() const
{
	if (errvec.empty()) {
		return std::string();
	} else {
		return std::string(&errvec[0], errvec.size());
	}
}

int Win32Process::run(const QString &command, QByteArray *out, QByteArray *err)
{
	QTextCodec *sjis = QTextCodec::codecForName("Shift_JIS");
	Q_ASSERT(sjis);

	QByteArray ba = sjis->fromUnicode(command);
	ba.push_back((char)0);
	char const *cmd = ba.data();

	int r = run(cmd);
	if (out) {
		out->clear();
		if (!outvec.empty()) {
			out->append(&outvec[0], outvec.size());
		}

	}
	if (err) {
		err->clear();
		if (!errvec.empty()) {
			err->append(&errvec[0], errvec.size());
		}
	}
	return r;
}
