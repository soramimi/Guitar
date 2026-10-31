#include "PtyProcessController.h"

#include <AbstractProcess.h>

#include <QElapsedTimer>
#include <QMetaObject>
#include <QTimer>

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace {

constexpr qsizetype maximum_pending_input_bytes = 1024 * 1024;
constexpr int output_timer_interval_ms = 10;
constexpr int output_drain_byte_budget = 64 * 1024;
constexpr int output_drain_time_budget_ms = 4;

struct CallbackGate {
	std::mutex mutex;
	PtyProcessController *controller = nullptr;
};

struct InputRequest {
	std::shared_ptr<AbstractPtyProcess> process;
	uint64_t session_id = 0;
	QByteArray input;
};

// A process backend is allowed to block while its child is not reading. Keep
// that behaviour off the GUI thread and retain the process shared_ptr in every
// request so a late request can never reach a subsequently started session.
class InputDispatcher : public std::enable_shared_from_this<InputDispatcher> {
public:
	static std::shared_ptr<InputDispatcher> create()
	{
		auto result = std::shared_ptr<InputDispatcher>(new InputDispatcher);
		std::thread([keep_alive = result](){ keep_alive->run(); }).detach();
		return result;
	}

	bool enqueue(InputRequest request)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (stopping_ || request.input.isEmpty()) return false;
		if (queued_bytes_ + request.input.size() > maximum_pending_input_bytes) return false;
		queued_bytes_ += request.input.size();
		requests_.push_back(std::move(request));
		changed_.notify_one();
		return true;
	}

	void discard(uint64_t session_id)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto it = requests_.begin(); it != requests_.end();) {
			if (it->session_id != session_id) {
				++it;
				continue;
			}
			queued_bytes_ -= it->input.size();
			it = requests_.erase(it);
		}
	}

	void shutdown()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stopping_ = true;
		requests_.clear();
		queued_bytes_ = 0;
		changed_.notify_one();
	}

private:
	void run()
	{
		for (;;) {
			InputRequest request;
			{
				std::unique_lock<std::mutex> lock(mutex_);
				changed_.wait(lock, [this](){ return stopping_ || !requests_.empty(); });
				if (stopping_) return;
				request = std::move(requests_.front());
				queued_bytes_ -= request.input.size();
				requests_.pop_front();
			}
			request.process->write_input(request.input.constData(), request.input.size());
		}
	}

	std::mutex mutex_;
	std::condition_variable changed_;
	std::deque<InputRequest> requests_;
	qsizetype queued_bytes_ = 0;
	bool stopping_ = false;
};

std::shared_ptr<AbstractPtyProcess> new_pty_process(PtyProcessBackend backend)
{
	return createPtyProcess(backend);
}

QString quote_command_argument(QString const &argument)
{
#ifdef Q_OS_WIN
	// CreateProcessW command-line escaping: a run of backslashes before a quote
	// is doubled, and a trailing run is doubled before the closing quote.
	QString result;
	result += QLatin1Char('"');
	int backslashes = 0;
	for (QChar const character : argument) {
		if (character == QLatin1Char('\\')) {
			backslashes++;
			continue;
		}
		if (character == QLatin1Char('"')) {
			result += QString(backslashes * 2 + 1, QLatin1Char('\\'));
			result += character;
			backslashes = 0;
			continue;
		}
		result += QString(backslashes, QLatin1Char('\\'));
		backslashes = 0;
		result += character;
	}
	result += QString(backslashes * 2, QLatin1Char('\\'));
	result += QLatin1Char('"');
	return result;
#else
	// ProcessPosixPty's parser uses "\"\"" for a literal quote. Keeping this
	// conversion here preserves empty arguments and UTF-8 path names as distinct
	// argv elements without asking a shell to interpret them.
	QString result = argument;
	result.replace(QLatin1Char('"'), QStringLiteral("\"\"\""));
	return QLatin1Char('"') + result + QLatin1Char('"');
#endif
}

}

struct PtyProcessController::Private {
	std::shared_ptr<AbstractPtyProcess> process;
	std::shared_ptr<InputDispatcher> input_dispatcher = InputDispatcher::create();
	std::shared_ptr<CallbackGate> callback_gate = std::make_shared<CallbackGate>();
	QTimer output_timer;
	QSize terminal_size = QSize(80, 25);
	PtyProcessBackend backend = PtyProcessBackend::Worker;
	State state = State::Idle;
	uint64_t session_id = 0;
	bool completion_pending = false;
	ProcessResult completion_result;
};

PtyProcessController::PtyProcessController(QObject *parent)
	: QObject(parent)
	, m(new Private)
{
	std::lock_guard<std::mutex> lock(m->callback_gate->mutex);
	m->callback_gate->controller = this;
	m->output_timer.setInterval(output_timer_interval_ms);
	connect(&m->output_timer, &QTimer::timeout, this, [this](){
		drainOutput(m->session_id);
	});
}

PtyProcessController::~PtyProcessController()
{
	// Completion and input workers retain shared ownership of their process. A
	// controller can therefore disappear without blocking the GUI or allowing a
	// queued callback to access this now-destroyed QObject.
	{
		std::lock_guard<std::mutex> lock(m->callback_gate->mutex);
		m->callback_gate->controller = nullptr;
	}
	m->output_timer.stop();
	m->input_dispatcher->shutdown();
	if (m->process && m->state != State::Stopping) {
		auto process = m->process;
		std::thread([process](){ process->stop(); }).detach();
	}
	delete m;
}

bool PtyProcessController::start(
	QString const &program,
	QStringList const &arguments,
	QSize terminal_size)
{
	if (program.isEmpty() || isRunning()) return false;

	m->process.reset();
	m->completion_pending = false;
	m->completion_result = {};
	m->session_id++;
	if (m->session_id == 0) m->session_id++;
	uint64_t const session_id = m->session_id;
	m->process = new_pty_process(m->backend);
	if (!m->process) return false;

	m->process->set_capture_output_vector(false);
	m->terminal_size = QSize(
		std::max(1, terminal_size.width()),
		std::max(1, terminal_size.height()));
	m->process->resize_terminal(m->terminal_size.width(), m->terminal_size.height());

	auto const gate = m->callback_gate;
	m->process->set_completion_callback(
		[gate, session_id](bool, std::shared_ptr<void>){
			std::lock_guard<std::mutex> lock(gate->mutex);
			if (!gate->controller) return;
			QMetaObject::invokeMethod(gate->controller, [gate, session_id](){
				PtyProcessController *controller = nullptr;
				{
					std::lock_guard<std::mutex> lock(gate->mutex);
					controller = gate->controller;
				}
				if (controller) controller->handleCompleted(session_id);
			}, Qt::QueuedConnection);
		}, {});

	QStringList command_parts;
	command_parts.reserve(arguments.size() + 1);
	command_parts.push_back(quote_command_argument(program));
	for (QString const &argument : arguments) {
		command_parts.push_back(quote_command_argument(argument));
	}
	std::string const command = command_parts.join(QLatin1Char(' ')).toUtf8().toStdString();
	std::string environment = "TERM=xterm-256color";
	environment.push_back('\0');
	environment += "COLORTERM=truecolor";

	m->state = State::Starting;
	emit stateChanged(m->state, session_id);
	m->output_timer.start();
	m->process->start(command, environment, true);
	return true;
}

void PtyProcessController::writeInput(QByteArray const &input)
{
	if (input.isEmpty()) return;
	if ((m->state != State::Starting && m->state != State::Running) || !m->process) {
		emit inputRejected(tr("端末プロセスは入力を受け付けていません"));
		return;
	}
	if (!m->input_dispatcher->enqueue({m->process, m->session_id, input})) {
		emit inputRejected(tr("端末入力キューの上限（1 MiB）を超えました"));
	}
}

void PtyProcessController::stop()
{
	if (!m->process || m->state == State::Stopping) return;
	if (m->state != State::Starting && m->state != State::Running) return;

	uint64_t const session_id = m->session_id;
	auto process = m->process;
	m->input_dispatcher->discard(session_id);
	m->state = State::Stopping;
	emit stateChanged(m->state, session_id);

	auto const gate = m->callback_gate;
	std::thread([process, gate, session_id](){
		process->stop();
		std::lock_guard<std::mutex> lock(gate->mutex);
		if (!gate->controller) return;
		QMetaObject::invokeMethod(gate->controller, [gate, session_id](){
			PtyProcessController *controller = nullptr;
			{
				std::lock_guard<std::mutex> lock(gate->mutex);
				controller = gate->controller;
			}
			if (controller) controller->handleCompleted(session_id);
		}, Qt::QueuedConnection);
	}).detach();
}

bool PtyProcessController::isRunning() const
{
	return m->state == State::Starting || m->state == State::Running
		|| m->state == State::Stopping;
}

PtyProcessController::State PtyProcessController::state() const
{
	return m->state;
}

uint64_t PtyProcessController::sessionId() const
{
	return m->session_id;
}

void PtyProcessController::setBackend(PtyProcessBackend backend)
{
	m->backend = backend;
}

void PtyProcessController::resizeTerminal(QSize size)
{
	m->terminal_size = QSize(std::max(1, size.width()), std::max(1, size.height()));
	if (!m->process || !isRunning()) return;

	auto process = m->process;
	int const columns = m->terminal_size.width();
	int const rows = m->terminal_size.height();
	std::thread([process, columns, rows](){
		process->resize_terminal(columns, rows);
	}).detach();
}

void PtyProcessController::drainOutput(uint64_t session_id)
{
	if (session_id != m->session_id || !m->process) return;
	if (m->state == State::Starting && m->process->is_running()) {
		m->state = State::Running;
		emit stateChanged(m->state, session_id);
	}

	QElapsedTimer elapsed;
	elapsed.start();
	char buffer[16384];
	int bytes_drained = 0;
	bool budget_exhausted = false;
	while (bytes_drained < output_drain_byte_budget) {
		int const capacity = std::min<int>(sizeof(buffer), output_drain_byte_budget - bytes_drained);
		int const size = m->process->read_output(buffer, capacity);
		if (size <= 0) break;
		bytes_drained += size;
		emit outputReady(QByteArray(buffer, size));
		if (elapsed.elapsed() >= output_drain_time_budget_ms) {
			budget_exhausted = true;
			break;
		}
	}
	if (bytes_drained >= output_drain_byte_budget) budget_exhausted = true;

	if (budget_exhausted) {
		QTimer::singleShot(0, this, [this, session_id](){ drainOutput(session_id); });
		return;
	}
	if (m->completion_pending) finishCompletedSession(session_id);
}

void PtyProcessController::handleCompleted(uint64_t session_id)
{
	if (session_id != m->session_id || !m->process || m->completion_pending) return;
	ProcessResult const result = m->process->wait(0);
	if (result.running()) {
		QTimer::singleShot(output_timer_interval_ms, this, [this, session_id](){
			handleCompleted(session_id);
		});
		return;
	}
	m->completion_result = result;
	m->completion_pending = true;
	m->input_dispatcher->discard(session_id);
	drainOutput(session_id);
}

void PtyProcessController::finishCompletedSession(uint64_t session_id)
{
	if (session_id != m->session_id || !m->completion_pending) return;
	ProcessResult const result = m->completion_result;
	m->completion_pending = false;
	m->output_timer.stop();
	m->process.reset();
	m->state = State::Exited;
	emit stateChanged(m->state, session_id);
	emit completed(static_cast<int>(result.exit_code()),
		QString::fromStdString(result.error_message()));
}
