#include "ProcessWinConPtyWithWorker.h"
#include "BasicProcessWinConPty.h"
#include "ConPtyWorkerProtocol.h"
#include "ProcessWinHelper.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <thread>
#include <windows.h>

namespace {
namespace wire = process::worker_protocol;
struct Handle {
	HANDLE value = nullptr;
	Handle() = default;
	Handle(Handle const &) = delete;
	Handle &operator=(Handle const &) = delete;
	~Handle() { close(); }
	void close()
	{
		if (IS_VALID_HANDLE(value)) CloseHandle(value);
		value = nullptr;
	}
};
void wincheck(bool ok)
{
	if (!ok) throw std::runtime_error(misc::get_error_message(GetLastError()));
}
// Poll before ReadFile so shutdown never depends on another process closing a pipe.
bool poll(HANDLE pipe, std::string &bytes)
{
	DWORD available = 0;
	if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
		if (GetLastError() == ERROR_BROKEN_PIPE) return false;
		throw std::runtime_error("Worker pipe: " + misc::get_error_message(GetLastError()));
	}
	if (available) {
		char data[16384];
		DWORD n = 0;
		wincheck(ReadFile(pipe, data, std::min<DWORD>(available, sizeof(data)), &n, nullptr));
		bytes.append(data, n);
	}
	return true;
}
std::string encode_result(ProcessResult const &r)
{
	std::string s;
	wire::put(s, r.started_);
	wire::put(s, r.exit_code_);
	wire::put(s, r.error_code_);
	wire::put_string(s, r.error_message_);
	return s;
}
ProcessResult decode_result(std::string_view s)
{
	ProcessResult r;
	r.started_ = wire::get(s) != 0;
	r.exit_code_ = wire::get(s);
	r.error_code_ = wire::get(s);
	r.error_message_ = wire::get_string(s);
	if (!s.empty()) throw std::runtime_error("Invalid worker result");
	return r;
}
struct Session {
	Handle process, job, input, output, status;
	std::mutex queue_mutex;
	std::condition_variable queue_changed;
	std::deque<std::string> queue;
	bool writer_done = false;
	std::thread writer;
	void finish_writer()
	{
		{
			std::lock_guard<std::mutex> lock(queue_mutex);
			writer_done = true;
		}
		queue_changed.notify_all();
		if (writer.joinable()) writer.join();
	}
	~Session() { finish_writer(); }
	void send(std::string message, bool priority = false)
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		if (writer_done) return;
		// Resize/stop must not be trapped behind queued terminal input.
		if (priority) {
			// Initialization must remain first, even if stop immediately follows start.
			auto position = queue.begin();
			if (position != queue.end() && static_cast<unsigned char>((*position)[8]) == wire::Init) ++position;
			queue.insert(position, std::move(message));
		} else
			queue.push_back(std::move(message));
		queue_changed.notify_one();
	}
};
void launch(Session &s, std::wstring const &path, std::wstring const &directory)
{
	Handle child_in, child_out, child_status, primary;
	SECURITY_ATTRIBUTES sa { sizeof(sa), nullptr, TRUE };
	wincheck(CreatePipe(&child_in.value, &s.input.value, &sa, 0));
	wincheck(CreatePipe(&s.output.value, &child_out.value, &sa, 0));
	wincheck(CreatePipe(&s.status.value, &child_status.value, &sa, 0));
	for (HANDLE h : { s.input.value, s.output.value, s.status.value })
		wincheck(SetHandleInformation(h, HANDLE_FLAG_INHERIT, 0));
	s.job.value = CreateJobObjectW(nullptr, nullptr);
	wincheck(s.job.value != nullptr);
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits { };
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	wincheck(SetInformationJobObject(s.job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)));
	SIZE_T size = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
	std::vector<unsigned char> storage(size);
	auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
	wincheck(InitializeProcThreadAttributeList(attributes, 1, 0, &size));
	struct Cleanup {
		LPPROC_THREAD_ATTRIBUTE_LIST p;
		~Cleanup() { DeleteProcThreadAttributeList(p); }
	} cleanup { attributes };
	HANDLE inherited[] { child_in.value, child_out.value, child_status.value };
	wincheck(UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
		inherited, sizeof(inherited), nullptr, nullptr));
	STARTUPINFOEXW si { };
	si.StartupInfo.cb = sizeof(si);
	si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	si.StartupInfo.hStdInput = child_in.value;
	si.StartupInfo.hStdOutput = child_out.value;
	si.StartupInfo.hStdError = child_status.value;
	si.lpAttributeList = attributes;
	std::wstring command = L"\"" + path + L"\" --conpty-worker-v1";
	PROCESS_INFORMATION pi { };
	wincheck(CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, TRUE,
		CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
		nullptr, directory.empty() ? nullptr : directory.c_str(), &si.StartupInfo, &pi));
	s.process.value = pi.hProcess;
	primary.value = pi.hThread;
	if (!AssignProcessToJobObject(s.job.value, pi.hProcess)) {
		DWORD error = GetLastError();
		TerminateProcess(pi.hProcess, 126);
		throw std::runtime_error("Assign worker job: " + misc::get_error_message(error));
	}
	wincheck(ResumeThread(pi.hThread) != DWORD(-1));
}
}

struct ProcessWinConPtyWithWorker::Private {
	std::mutex state;
	std::condition_variable changed;
	ProcessResult result;
	std::shared_ptr<Session> session;
	std::thread reader;
	std::wstring worker_path;
	int columns = 80, rows = 25;
	bool input_closed = false;
};
ProcessWinConPtyWithWorker::ProcessWinConPtyWithWorker()
	: m(new Private)
{
}
ProcessWinConPtyWithWorker::~ProcessWinConPtyWithWorker() { stop(); }
void ProcessWinConPtyWithWorker::set_worker_path(process::helper::dir_string_t const &path)
{
	std::lock_guard<std::mutex> lock(m->state);
	m->worker_path = path;
}
void ProcessWinConPtyWithWorker::start(std::string const &command, std::string const &env, bool use_input)
{
	stop();
	reset_output();
	std::shared_ptr<Session> session = std::make_shared<Session>();
	std::string init;
	try {
		if (command.empty() || command.find('\0') != std::string::npos)
			throw std::runtime_error("Empty command or embedded NUL");
		std::wstring path;
		{
			std::lock_guard<std::mutex> lock(m->state);
			m->result = { };
			m->input_closed = !use_input;
			path = m->worker_path;
			wire::put(init, m->columns);
			wire::put(init, m->rows);
			wire::put(init, use_input);
		}
		wire::put_string(init, command);
		wire::put_string(init, env);
		init = wire::frame(wire::Init, init);
		if (path.empty()) {
			std::vector<wchar_t> buffer(32768);
			DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
			if (!n || n >= buffer.size()) throw std::runtime_error("Cannot locate host executable");
			path.assign(buffer.data(), n);
			path.erase(path.find_last_of(L"/\\") + 1);
			path += L"conpty-worker.exe";
		}
		launch(*session, path, change_dir_);
	} catch (std::exception const &e) {
		if (session->job.value) TerminateJobObject(session->job.value, 126);
		{
			std::lock_guard<std::mutex> lock(m->state);
			m->result = { };
			m->result.error_code_ = ERROR_PROCESS_ABORTED;
			m->result.error_message_ = e.what();
		}
		m->changed.notify_all();
		{
			std::lock_guard<std::mutex> output_lock(mutex_);
			cond_.notify_all();
		}
		notify_completed();
		return;
	}
	session->send(std::move(init));
	{
		std::lock_guard<std::mutex> lock(m->state);
		m->session = session;
		m->result.started_ = true;
		m->result.running_ = true;
	}
	session->writer = std::thread([session] {
		for (;;) {
			std::string message;
			{
				std::unique_lock<std::mutex> lock(session->queue_mutex);
				session->queue_changed.wait(lock, [&] { return session->writer_done || !session->queue.empty(); });
				if (session->writer_done) break;
				message = std::move(session->queue.front());
				session->queue.pop_front();
			}
			if (!write_all(session->input.value, message.data(), message.size())) break;
		}
	});
	m->reader = std::thread([this, session] {
		ProcessResult result;
		std::string status;
		bool output_open = true, status_open = true, received = false, killed_descendants = false;
		try {
			while (output_open || status_open || WaitForSingleObject(session->process.value, 0) == WAIT_TIMEOUT) {
				std::string data;
				if (output_open) output_open = poll(session->output.value, data);
				if (!data.empty()) {
					write_output(data.data(), data.size());
					cond_.notify_all();
				}
				if (status_open) status_open = poll(session->status.value, status);
				wire::Type type;
				std::string payload;
				while (wire::take(status, type, payload)) {
					if (type != wire::Result || received) throw std::runtime_error("Unexpected worker response");
					result = decode_result(payload);
					received = true;
				}
				if (!killed_descendants && WaitForSingleObject(session->process.value, 0) == WAIT_OBJECT_0) {
					// A grandchild must not keep inherited output handles or survive its owner.
					TerminateJobObject(session->job.value, 1);
					killed_descendants = true;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
			if (!received || !status.empty()) throw std::runtime_error("Worker exited without a complete result");
		} catch (std::exception const &e) {
			result.error_code_ = ERROR_PROCESS_ABORTED;
			result.error_message_ = e.what();
			TerminateJobObject(session->job.value, 126);
		}
		session->finish_writer();
		{
			std::lock_guard<std::mutex> lock(m->state);
			result.running_ = false;
			m->result = result;
		}
		m->changed.notify_all();
		{
			std::lock_guard<std::mutex> output_lock(mutex_);
			cond_.notify_all();
		}
		notify_completed();
	});
}
ProcessResult ProcessWinConPtyWithWorker::wait(int time)
{
	std::unique_lock<std::mutex> lock(m->state);
	if (time == INT_MAX)
		m->changed.wait(lock, [&] { return !m->result.running_; });
	else
		m->changed.wait_for(lock, std::chrono::milliseconds(std::max(0, time)), [&] { return !m->result.running_; });
	return m->result;
}
void ProcessWinConPtyWithWorker::stop()
{
	std::shared_ptr<Session> session;
	{
		std::lock_guard<std::mutex> lock(m->state);
		session = m->session;
	}
	if (session && is_running()) {
		session->send(wire::frame(wire::Stop), true);
		if (wait(2000).running_) TerminateJobObject(session->job.value, 1);
		wait();
	}
	if (m->reader.joinable()) m->reader.join();
	{
		std::lock_guard<std::mutex> lock(m->state);
		m->session.reset();
	}
}
bool ProcessWinConPtyWithWorker::is_running() const
{
	std::lock_guard<std::mutex> lock(m->state);
	return m->result.running_;
}
int ProcessWinConPtyWithWorker::get_exit_code() const
{
	std::lock_guard<std::mutex> lock(m->state);
	return static_cast<int>(m->result.exit_code_);
}
void ProcessWinConPtyWithWorker::write_input(char const *ptr, int len)
{
	if (!ptr || len <= 0) return;
	std::lock_guard<std::mutex> lock(m->state);
	if (!m->result.running_ || m->input_closed) return;
	// Small frames allow control requests to interrupt a long queued input.
	while (len > 0) {
		int n = std::min(len, 4096);
		m->session->send(wire::frame(wire::Input, std::string_view(ptr, n)));
		ptr += n;
		len -= n;
	}
}
int ProcessWinConPtyWithWorker::read_output(char *ptr, int len) { return pop_output(ptr, len); }
void ProcessWinConPtyWithWorker::close_input()
{
	std::lock_guard<std::mutex> lock(m->state);
	if (m->result.running_ && !m->input_closed) {
		m->input_closed = true;
		m->session->send(wire::frame(wire::CloseInput));
	}
}
void ProcessWinConPtyWithWorker::resize_terminal(int columns, int rows)
{
	std::lock_guard<std::mutex> lock(m->state);
	m->columns = std::clamp(columns, 1, 32767);
	m->rows = std::clamp(rows, 1, 32767);
	if (m->result.running_) {
		std::string s;
		wire::put(s, m->columns);
		wire::put(s, m->rows);
		m->session->send(wire::frame(wire::Resize, s));
	}
}
bool ProcessWinConPtyWithWorker::wait_for_output(std::string const &text)
{
	// Monitoring requires capture_output_vector; retain compatibility without consuming the queue.
	std::unique_lock<std::mutex> lock(mutex_);
	if (!capture_output_vector_) return false;
	size_t searched = 0;
	bool found = false;
	cond_.wait(lock, [&] {
		size_t begin = searched > text.size() ? searched - text.size() : 0;
		if (text.empty())
			found = true;
		else if (!output_vector_.empty())
			found = std::string_view(output_vector_.data() + begin,
						output_vector_.size() - begin)
						.find(text)
				!= std::string_view::npos;
		searched = output_vector_.size();
		return found || !is_running();
	});
	return found;
}

int ProcessWinConPtyWithWorker::run_worker(int argc, char **argv)
{
	if (argc != 2 || std::string_view(argv[1]) != "--conpty-worker-v1") return 126;
	HANDLE control = GetStdHandle(STD_INPUT_HANDLE), output = GetStdHandle(STD_OUTPUT_HANDLE), status = GetStdHandle(STD_ERROR_HANDLE);
	// Do not let console clients inherit the worker's protocol/output pipe handles as stdio.
	SetStdHandle(STD_INPUT_HANDLE, nullptr);
	SetStdHandle(STD_OUTPUT_HANDLE, nullptr);
	SetStdHandle(STD_ERROR_HANDLE, nullptr);
	BasicProcessWinConPty terminal;
	std::thread waiter;
	std::atomic<bool> done { false };
	struct Join {
		std::thread &thread;
		~Join()
		{
			if (thread.joinable()) thread.join();
		}
	} join { waiter };
	BasicProcessWinConPty::Options options;
	options.no_window = false;
	options.vt_stripped = false;
	options.output_queue = true;
	options.forward_parent_stdin = false;
	terminal.set_options(options);
	ProcessResult result;
	try {
		std::string buffer, payload;
		wire::Type type;
		bool initialized = false, allow_input = false;
		for (;;) {
			bool connected = poll(control, buffer);
			while (wire::take(buffer, type, payload)) {
				std::string_view s(payload);
				if (!initialized) {
					if (type != wire::Init) throw std::runtime_error("Worker expects initialization");
					auto columns = wire::get(s), rows = wire::get(s);
					allow_input = wire::get(s) != 0;
					std::string command = wire::get_string(s), env = wire::get_string(s);
					if (!s.empty() || columns < 1 || columns > 32767 || rows < 1 || rows > 32767
						|| command.empty() || command.find('\0') != std::string::npos) throw std::runtime_error("Invalid worker initialization");
					terminal.resize_terminal(columns, rows);
					terminal.set_environment(env);
					if (!terminal.start(command)) {
						result = terminal.wait();
						done = true;
					} else
						waiter = std::thread([&] { result = terminal.wait(); done = true; });
					initialized = true;
				} else if (type == wire::Input && allow_input) {
					terminal.write_input(payload.data(), static_cast<int>(payload.size()));
				} else if (type == wire::Resize) {
					auto columns = wire::get(s), rows = wire::get(s);
					if (!s.empty() || columns < 1 || columns > 32767 || rows < 1 || rows > 32767) throw std::runtime_error("Invalid worker resize");
					terminal.resize_terminal(columns, rows);
				} else if (type == wire::CloseInput && payload.empty()) {
					allow_input = false;
					terminal.close_input();
				} else if (type == wire::Stop && payload.empty()) {
					terminal.terminate();
				} else
					throw std::runtime_error("Unexpected worker request");
			}
			char bytes[16384];
			int n;
			while ((n = terminal.read_output(bytes, sizeof(bytes))) > 0)
				if (!write_all(output, bytes, n)) throw std::runtime_error("Host output disconnected");
			if (!connected) {
				terminal.terminate();
				throw std::runtime_error("Host control disconnected");
			}
			if (initialized && done) {
				if (waiter.joinable()) waiter.join();
				while ((n = terminal.read_output(bytes, sizeof(bytes))) > 0)
					if (!write_all(output, bytes, n)) throw std::runtime_error("Host output disconnected");
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
	} catch (std::exception const &e) {
		terminal.terminate();
		if (waiter.joinable()) waiter.join();
		result.error_code_ = ERROR_PROCESS_ABORTED;
		result.error_message_ = e.what();
	}
	auto message = wire::frame(wire::Result, encode_result(result));
	return write_all(status, message.data(), message.size()) ? 0 : 126;
}
