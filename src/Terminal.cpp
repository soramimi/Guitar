#include "Terminal.h"

#include <QFileInfo>

#ifdef Q_OS_WIN

#include <windows.h>

void Terminal::open(QString const &dir)
{
	if (dir.indexOf('\"') < 0 && QFileInfo(dir).isDir()) {
		QString cmd = "cmd.exe /k \"cd %1\"";
		cmd = cmd.arg(dir);

		PROCESS_INFORMATION pi;
		STARTUPINFO si;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);

		CreateProcess(nullptr, (wchar_t *)cmd.utf16(), nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS, nullptr, nullptr, &si, &pi);

		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
}

#else

void Terminal::open(QString const &dir)
{
	if (dir.indexOf('\"') < 0 && QFileInfo(dir).isDir()) {
		auto GetTerm = [&](std::vector<char const *> const &vec){
			for (char const *name : vec) {
				char const *p = getenv(name);
				if (p && *p) return p;
			}
			return "xterm";
		};
		QString term = GetTerm({"COLORTERM", "TERM"});

		QString cmd = "/bin/sh -c \"cd \\\"%1\\\" && %2\" &";
		cmd = cmd.arg(dir).arg(term);
		system(cmd.toStdString().c_str());
	}
}

#endif
