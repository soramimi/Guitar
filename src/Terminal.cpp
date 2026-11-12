
#include "ApplicationGlobal.h"
#include "Terminal.h"
#include <QFileInfo>

#ifdef Q_OS_WIN

#include <QDir>
#include <windows.h>

void Terminal::open(QString const &dir, QString const &ssh_key)
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

#include <QProcess>
#include <QProcessEnvironment>

namespace {
// Quote a single word for POSIX sh. Git runs GIT_SSH_COMMAND through the shell,
// so the key path must not be able to inject words or expansions there.
QString shellQuote(QString const &s)
{
	QString t = s;
	t.replace('\'', "'\\''");
	return '\'' + t + '\'';
}
} // namespace

void Terminal::open(QString const &dir, QString const &ssh_key)
{
	if (!QFileInfo(dir).isDir()) return;

	QString term = global->appsettings.terminal_command;
	if (term.isEmpty()) {
		term = ApplicationSettings::defaultSettings().terminal_command;
	}

	// The terminal setting is the user's own shell command line (it may carry
	// options or quoting), so it is still run by /bin/sh as before. The
	// repository directory is never embedded in that text: it is passed as the
	// working directory, so names containing $(...), backquotes or %N are inert.
	QProcess proc;
	proc.setProgram("/bin/sh");
	proc.setArguments({"-c", term});
	proc.setWorkingDirectory(dir);
	if (!global->appsettings.ssh_command.isEmpty() && !ssh_key.isEmpty()) {
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
		env.insert("GIT_SSH_COMMAND", global->appsettings.ssh_command + " -i " + shellQuote(ssh_key));
		proc.setProcessEnvironment(env);
	}
	proc.startDetached();
}

#endif
