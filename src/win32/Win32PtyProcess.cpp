#include <windows.h>
#include "Win32PtyProcess.h"
#include <deque>

#include <winpty.h>

namespace {

class OutputReaderThread : public QThread {
	friend class Win32PtyProcess;
private:
	HANDLE handle;
	std::deque<char> result1;
	std::vector<char> result2;
protected:
	void run();
public:
	void start(HANDLE hOutput);
	std::vector<char> const *result() const
	{
		return &result2;
	}
};

void OutputReaderThread::run()
{
	char buf[1024];
	while (1) {
		if (isInterruptionRequested()) break;
		DWORD amount = 0;
		BOOL ret = ReadFile(handle, buf, sizeof(buf), &amount, nullptr);
		if (!ret || amount == 0) {
			break;
		}
		result1.insert(result1.end(), buf, buf + amount);
		result2.insert(result2.end(), buf, buf + amount);
	}
}

void OutputReaderThread::start(HANDLE hOutput)
{
	result2.clear();
	handle = hOutput;
	QThread::start();
}

} // namespace

// Win32PtyProcess

struct Win32PtyProcess::Private {
	QString command;
	OutputReaderThread th_output_reader;
	HANDLE hProcess = INVALID_HANDLE_VALUE;
	HANDLE hOutput = INVALID_HANDLE_VALUE;
	HANDLE hInput = INVALID_HANDLE_VALUE;
	DWORD exit_code = 0;
};

Win32PtyProcess::Win32PtyProcess()
	: m(new Private)
{
}

Win32PtyProcess::~Win32PtyProcess()
{
	delete m;
}

QString Win32PtyProcess::getProgram(const QString &cmdline)
{
	ushort const *begin = cmdline.utf16();
	ushort const *end = begin + cmdline.size();
	ushort const *ptr = begin;
	bool quote = 0;
	while (1) {
		ushort c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (c == '\"') {
			if (quote) {
				quote = false;
			} else {
				quote = true;
			}
			ptr++;
		} else if (quote && c != 0) {
			ptr++;
		} else if (QChar(c).isSpace() || c == 0) {
			break;
		} else {
			ptr++;
		}
	}
	ushort const *left = begin;
	ushort const *right = ptr;
	if (left + 1 < right) {
		if (left[0] == '\"' && right[-1] == '\"') {
			left++;
			right--;
		}
	}
	return QString::fromUtf16(left, right - left);
}

void Win32PtyProcess::run()
{
	QString program;
	program = getProgram(m->command);

	winpty_config_t *agent_cfg = winpty_config_new(WINPTY_FLAG_PLAIN_OUTPUT, nullptr);
	winpty_t *pty = winpty_open(agent_cfg, nullptr);
	winpty_config_free(agent_cfg);

	m->hInput = CreateFileW(winpty_conin_name(pty), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	m->hOutput = CreateFileW(winpty_conout_name(pty), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	m->th_output_reader.start(m->hOutput);

	winpty_spawn_config_t *spawn_cfg = winpty_spawn_config_new(WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN, (wchar_t const *)program.utf16(), (wchar_t const *)m->command.utf16(), nullptr, nullptr, nullptr);
	BOOL spawnSuccess = winpty_spawn(pty, spawn_cfg, &m->hProcess, nullptr, nullptr, nullptr);

	if (spawnSuccess) {
		while (1) {
			if (isInterruptionRequested()) break;
			GetExitCodeProcess(m->hProcess, &m->exit_code);
			if (m->exit_code == STILL_ACTIVE) {
				// running
				msleep(1);
			} else {
				break;
			}
		}
	}

	close();
	winpty_free(pty);
}

int Win32PtyProcess::readOutput(char *dstptr, int maxlen)
{
	int len = m->th_output_reader.result1.size();
	if (len > maxlen) {
		len = maxlen;
	}
	if (len > 0) {
		auto begin = m->th_output_reader.result1.begin();
		std::copy(begin, begin + len, dstptr);
		m->th_output_reader.result1.erase(begin, begin + len);
	}
	return len;
}

void Win32PtyProcess::writeInput(const char *ptr, int len)
{
	char const *begin = ptr;
	char const *end = begin + len;
	char const *left = begin;
	char const *right = begin;
	while (1) {
		int c = -1;
		if (right < end) {
			c = *right & 0xff;
		}
		if (c == '\r' || c == '\n' || c < 0) {
			if (left < right) {
				DWORD written;
				WriteFile(m->hInput, left, right - left, &written, 0);
			}
			if (c < 0) break;
			right++;
			if (c == '\r') {
				if (*right == '\n') {
					right++;
				}
				c = '\r';
			} else if (c == '\n') {
				c = '\r';
			} else {
				c = -1;
			}
			if (c >= 0) {
				DWORD written;
				WriteFile(m->hInput, &c, 1, &written, 0);
			}
			left = right;
		} else {
			right++;
		}
	}
}

void Win32PtyProcess::close()
{
	CloseHandle(m->hInput);
	CloseHandle(m->hOutput);
	CloseHandle(m->hProcess);
	m->hInput = INVALID_HANDLE_VALUE;
	m->hOutput = INVALID_HANDLE_VALUE;
	m->hProcess = INVALID_HANDLE_VALUE;
}

void Win32PtyProcess::start(const QString &cmdline)
{
	if (isRunning()) return;
	m->command = cmdline;
	QThread::start();
}

void Win32PtyProcess::stop()
{
	m->th_output_reader.requestInterruption();
	requestInterruption();
	close();
	m->th_output_reader.wait();
	wait();
}

int Win32PtyProcess::wait()
{
	QThread::wait();
	return m->exit_code;
}

const std::vector<char> *Win32PtyProcess::result() const
{
	return m->th_output_reader.result();
}



