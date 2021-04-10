#include "Terminal.h"
#include "ApplicationGlobal.h"
#include <QFileInfo>

#ifdef Q_OS_WIN

#include <windows.h>

void Terminal::open(QString const &dir)
{
	if (dir.indexOf('\"') < 0 && QFileInfo(dir).isDir()) {
		QString arg;
		if (dir.at(0).isLetter() && dir.at(1) == ':') {
			arg = QString("%1 & cd %2").arg(dir.mid(0, 2)).arg(dir);
		} else {
			arg = QString("cd %1").arg(dir);
		}
		QString cmd = "cmd.exe /k \"%1\"";
		cmd = cmd.arg(arg);

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
		QString term = global->appsettings.terminal_command;
		QString cmd = "/bin/sh -c \"cd \\\"%1\\\" && %2\" &";
		cmd = cmd.arg(dir).arg(term);
		int r = system(cmd.toStdString().c_str());
		(void)r;
	}
}

#endif
