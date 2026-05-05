#include <windows.h>
#include "Win32PtyProcess.h"
#include <TraceLogger.h>
#include <QDir>
#include <QElapsedTimer>
#include <QMutex>
#include <deque>
#include <thread>
#include <winpty.h>
#include "common/misc.h"

namespace {

class OutputReaderThread : public QThread {
	friend class ::Win32PtyProcess;
private:
public:
	HANDLE handle_ = INVALID_HANDLE_VALUE;
	std::deque<char> *output_queue_ = nullptr;
	std::vector<char> *output_vector_ = nullptr;
protected:
	void run() override;
public:
	void start(HANDLE hOutput, std::deque<char> *outq, std::vector<char> *outv);
	void closeHandle()
	{
		if (handle_ != INVALID_HANDLE_VALUE) {
			CloseHandle(handle_);
			handle_ = INVALID_HANDLE_VALUE;
		}
	}
};

void OutputReaderThread::run()
{
	char buf[1024];
	while (1) {
		if (isInterruptionRequested()) break;
		DWORD amount = 0;
		BOOL ret = ReadFile(handle_, buf, sizeof(buf), &amount, nullptr);
		if (!ret || amount == 0) {
			break;
		}
		output_queue_->insert(output_queue_->end(), buf, buf + amount);
		output_vector_->insert(output_vector_->end(), buf, buf + amount);
	}
}

void OutputReaderThread::start(HANDLE hOutput, std::deque<char> *outq, std::vector<char> *outv)
{
	handle_ = hOutput;
	output_queue_ = outq;
	output_vector_ = outv;
	QThread::start();
}

} // namespace

// Win32PtyProcess

struct Win32PtyProcess::Private {
	QMutex mutex;
	std::string command;
	std::string env;
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

void Win32PtyProcess::run()
{
	QString cmd = QString::fromStdString(m->command);
	QString env = QString::fromStdString(m->env);

	std::wstring program;
	wchar_t const *program_p = nullptr;
	if (1) {
		// コマンドから実行ファイル名を抜き取る。実際に実行されるプログラムのパス。
		program = QString::fromStdString(misc::getProgram(m->command)).toStdWString();
		if (!program.empty()) {
			program_p = program.c_str();
		}
	} else {
		// nop:
		// program_p が nullptr 空の時、PATHが通っているコマンドなら実行できる。
	}

	QElapsedTimer timer;
	timer.start();

	QString cwd = QDir::currentPath();
	if (!change_dir_.isEmpty()) {
		QDir::setCurrent(change_dir_);
	}

	winpty_config_t *agent_cfg = winpty_config_new(WINPTY_FLAG_PLAIN_OUTPUT, nullptr);
	winpty_t *pty = winpty_open(agent_cfg, nullptr);
	winpty_config_free(agent_cfg);

	m->hInput = CreateFileW(winpty_conin_name(pty), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	m->hOutput = CreateFileW(winpty_conout_name(pty), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	m->th_output_reader.start(m->hOutput, &output_queue_, &output_vector_);

	TraceLogger trace;
	trace.begin("process", cmd);

	std::vector<wchar_t> envbuf;
	if (!env.isEmpty()) {
		envbuf.resize(env.size() + 1);
		memcpy(envbuf.data(), env.utf16(), sizeof(wchar_t) * (env.size() + 1));
	}

	winpty_spawn_config_t *spawn_cfg = winpty_spawn_config_new(WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN,
															   program_p,
															   (wchar_t const *)cmd.utf16(),
															   nullptr,
															   envbuf.empty() ? nullptr : envbuf.data(),
															   nullptr);
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

	trace.end();

	CloseHandle(m->hInput);
	CloseHandle(m->hOutput);
	CloseHandle(m->hProcess);
	m->hInput = INVALID_HANDLE_VALUE;
	m->hOutput = INVALID_HANDLE_VALUE;
	m->hProcess = INVALID_HANDLE_VALUE;

	QDir::setCurrent(cwd);

	qDebug() << "--- Win32PtyProcess\t" << m->command << "\t" << timer.elapsed() << "\t---";

	notifyCompleted();
}

int Win32PtyProcess::readOutputStreaming(char *dstptr, int maxlen)
{
	int len = output_queue_.size();
	if (len > maxlen) {
		len = maxlen;
	}
	if (len > 0) {
		auto begin = output_queue_.begin();
		std::copy(begin, begin + len, dstptr);
		output_queue_.erase(begin, begin + len);
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

void Win32PtyProcess::start(std::string const &cmdline, std::string const &env, bool use_input)
{
	(void)use_input;
	if (isRunning()) return;
	m->command = cmdline;
	m->env = env;
	QThread::start();
}

bool Win32PtyProcess::wait(unsigned long time)
{
	if (QThread::wait(time) && m->th_output_reader.wait(time)) {
		stdout_bytes_ = output_vector_;
		stderr_bytes_ = {};
		return true;
	}
	return false;
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

