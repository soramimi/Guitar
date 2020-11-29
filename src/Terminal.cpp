#include "Terminal.h"

#include <QFileInfo>

#include "ApplicationGlobal.h"

#ifdef Q_OS_WIN

#include <QDir>
#include <windows.h>

void Terminal::open(QString const &dir)
{
	QString cmd = global->appsettings.terminal_command;
	QDir::setCurrent(dir);
	if (dir.indexOf('\"') < 0 && QFileInfo(dir).isDir()) {
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
			#ifndef __HAIKU__
				for (char const *name : vec) {
					char const *p = getenv(name);
					if (p && *p) return p;
				}
				return "x-terminal-emulator";
			#else
				return "Terminal";
			#endif
		};
		QString term = GetTerm({"COLORTERM", "TERM"});

		QString cmd = "/bin/sh -c \"cd \\\"%1\\\" && %2\" &";
		cmd = cmd.arg(dir).arg(term);
		int r = system(cmd.toStdString().c_str());
		(void)r;
	}
}

#endif
