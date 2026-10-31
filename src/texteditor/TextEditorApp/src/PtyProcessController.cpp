#include "PtyProcessController.h"

#include <AbstractProcess.h>

#include <QMetaObject>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <memory>
#include <string>

struct PtyProcessController::Private {
	std::shared_ptr<AbstractPtyProcess> process;
	QTimer output_timer;
	QSize terminal_size = QSize(80, 25);
	std::atomic<bool> active { false };
	PtyProcessBackend backend = PtyProcessBackend::Direct;
};

namespace {

std::shared_ptr<AbstractPtyProcess> new_pty_process(PtyProcessBackend backend)
{
	return createPtyProcess(backend);
}

QString quote_command_argument(QString argument)
{
	argument.replace('"', "\"\"\"");
	return '"' + argument + '"';
}

}

PtyProcessController::PtyProcessController(QObject *parent)
	: QObject(parent)
	, m(new Private)
{
	m->output_timer.setInterval(10);
	connect(&m->output_timer, &QTimer::timeout, this, [this](){
		drainOutput();
	});
}

PtyProcessController::~PtyProcessController()
{
	stop();
	delete m;
}

bool PtyProcessController::start(
	QString const &program,
	QStringList const &arguments,
	QSize terminal_size)
{
	if (program.isEmpty() || m->active) return false;

	m->process = new_pty_process(m->backend);
	m->process->set_capture_output_vector(false);
	m->terminal_size = QSize(
		std::max(1, terminal_size.width()),
		std::max(1, terminal_size.height()));
	m->process->resize_terminal(m->terminal_size.width(), m->terminal_size.height());

	QPointer<PtyProcessController> self(this);
	m->process->set_completion_callback(
		[self](bool, std::shared_ptr<void>){
			if (!self) return;
			QMetaObject::invokeMethod(self.data(), [self](){
				if (self) self->handleCompleted();
			}, Qt::QueuedConnection);
		}, {});

	QStringList command_parts;
	command_parts.push_back(quote_command_argument(program));
	for (QString const &argument : arguments) {
		command_parts.push_back(quote_command_argument(argument));
	}
	std::string const command = command_parts.join(' ').toUtf8().toStdString();
	std::string environment = "TERM=xterm-256color";
	environment.push_back('\0');
	environment += "COLORTERM=truecolor";
	m->active = true;
	m->output_timer.start();
	m->process->start(command, environment, true);
	return true;
}

void PtyProcessController::stop()
{
	m->output_timer.stop();
	if (!m->process) {
		m->active = false;
		return;
	}
	m->active = false;
	m->process->stop();
	drainOutput();
	m->process.reset();
}

bool PtyProcessController::isRunning() const
{
	return m->active;
}

void PtyProcessController::setBackend(PtyProcessBackend backend)
{
	m->backend = backend;
}

void PtyProcessController::resizeTerminal(QSize size)
{
	m->terminal_size = QSize(std::max(1, size.width()), std::max(1, size.height()));
	if (m->process) {
		m->process->resize_terminal(m->terminal_size.width(), m->terminal_size.height());
	}
}

void PtyProcessController::drainOutput()
{
	if (!m->process) return;
	char buffer[16384];
	while (true) {
		int const size = m->process->read_output(buffer, sizeof(buffer));
		if (size <= 0) break;
		emit outputReady(QByteArray(buffer, size));
	}
}

void PtyProcessController::handleCompleted()
{
	if (!m->process || !m->active.exchange(false)) return;
	drainOutput();
	ProcessResult const result = m->process->wait(1000);
	drainOutput();
	m->output_timer.stop();
	QString const error_message = QString::fromStdString(result.error_message());
	emit completed(static_cast<int>(result.exit_code()), error_message);
}
