#include <windows.h>
#include "Win32PtyProcess.h"
#include <deque>

#include <winpty.h>

#include <QDir>
#include <QMutex>

namespace {

class OutputReaderThread : public QThread {
	friend class ::Win32PtyProcess;
private:
public:
	HANDLE handle;
	std::deque<char> *output_queue = nullptr;
	std::vector<char> *output_vector = nullptr;
protected:
	void run() override;
public:
	void start(HANDLE hOutput, std::deque<char> *outq, std::vector<char> *outv);
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
		output_queue->insert(output_queue->end(), buf, buf + amount);
		output_vector->insert(output_vector->end(), buf, buf + amount);
	}
}

void OutputReaderThread::start(HANDLE hOutput, std::deque<char> *outq, std::vector<char> *outv)
{
	handle = hOutput;
	output_queue = outq;
	output_vector = outv;
	QThread::start();
}

} // namespace

// Win32PtyProcess

struct Win32PtyProcess::Private {
	QMutex mutex;
	QString command;
	QString env;
	std::deque<char> output_queue;
	std::vector<char> output_vector;
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

bool Win32PtyProcess::isRunning() const
{
	return QThread::isRunning();
}

QString Win32PtyProcess::getProgram(QString const &cmdline)
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

	QString cwd = QDir::currentPath();
	QDir::setCurrent(change_dir);

	winpty_config_t *agent_cfg = winpty_config_new(WINPTY_FLAG_PLAIN_OUTPUT, nullptr);
	winpty_t *pty = winpty_open(agent_cfg, nullptr);
	winpty_config_free(agent_cfg);

	m->hInput = CreateFileW(winpty_conin_name(pty), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	m->hOutput = CreateFileW(winpty_conout_name(pty), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	m->th_output_reader.start(m->hOutput, &m->output_queue, &m->output_vector);

	std::vector<wchar_t> envbuf;
	if (!m->env.isEmpty()) {
		envbuf.resize(m->env.size() + 2);
		memcpy(envbuf.data(), m->env.utf16(), sizeof(wchar_t) * m->env.size());
	}
	winpty_spawn_config_t *spawn_cfg = winpty_spawn_config_new(WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN, (wchar_t const *)program.utf16(), (wchar_t const *)m->command.utf16(), nullptr, envbuf.empty() ? nullptr : envbuf.data(), nullptr);
	BOOL spawnSuccess = winpty_spawn(pty, spawn_cfg, &m->hProcess, nullptr, nullptr, nullptr);

	bool ok = false;

	if (spawnSuccess) {
		while (1) {
			if (isInterruptionRequested()) break;
			GetExitCodeProcess(m->hProcess, &m->exit_code);
			if (m->exit_code == STILL_ACTIVE) {
				// running
				msleep(1);
			} else {
				ok = true;
				break;
			}
		}
	}

	// プロセスの出力を確実に取得するため、ここで output reader スレッドの終了を待つ
	m->th_output_reader.wait();

	winpty_free(pty);

	CloseHandle(m->hInput);
	CloseHandle(m->hOutput);
	CloseHandle(m->hProcess);
	m->hInput = INVALID_HANDLE_VALUE;
	m->hOutput = INVALID_HANDLE_VALUE;
	m->hProcess = INVALID_HANDLE_VALUE;

	emit completed(ok, user_data);

	QDir::setCurrent(cwd);
}

int Win32PtyProcess::readOutput(char *dstptr, int maxlen)
{
	int len = m->output_queue.size();
	if (len > maxlen) {
		len = maxlen;
	}
	if (len > 0) {
		auto begin = m->output_queue.begin();
		std::copy(begin, begin + len, dstptr);
		m->output_queue.erase(begin, begin + len);
	}
	return len;
}

void Win32PtyProcess::writeInput(char const *ptr, int len)
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
				WriteFile(m->hInput, left, right - left, &written, nullptr);
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
				WriteFile(m->hInput, &c, 1, &written, nullptr);
			}
			left = right;
		} else {
			right++;
		}
	}
}

void Win32PtyProcess::start(QString const &cmdline, QString const &env, QVariant const &userdata)
{
	if (isRunning()) return;
	m->command = cmdline;
	m->env = env;
	this->user_data = userdata;
	QThread::start();
}

bool Win32PtyProcess::wait(unsigned long time)
{
	return QThread::wait(time) && m->th_output_reader.wait(time);
}

void Win32PtyProcess::stop()
{
	// 標準出力読み出しスレッドを強制終了しないとwinptyプロセスが終了してくれない
	m->th_output_reader.terminate();
	// プロセススレッド停止
	requestInterruption();
	wait();
}

int Win32PtyProcess::getExitCode() const
{
	return m->exit_code;
}

QString Win32PtyProcess::getMessage() const
{
	QString s;
	if (!m->output_vector.empty()) {
		s = QString::fromUtf8(&m->output_vector[0], m->output_vector.size());
	}
	return s;
}

void Win32PtyProcess::readResult(std::vector<char> *out)
{
	*out = m->output_vector;
	m->output_vector.clear();
}



