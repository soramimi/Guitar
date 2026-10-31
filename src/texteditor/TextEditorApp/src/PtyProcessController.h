#ifndef PTYPROCESSCONTROLLER_H
#define PTYPROCESSCONTROLLER_H

#include "PtyProcessFactory.h"

#include <QByteArray>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>

#include <cstdint>

class PtyProcessController : public QObject {
	Q_OBJECT
private:
	struct Private;
	Private *m;
	void drainOutput(uint64_t session_id);
	void handleCompleted(uint64_t session_id);
	void finishCompletedSession(uint64_t session_id);
public:
	enum class State {
		Idle,
		Starting,
		Running,
		Stopping,
		Exited,
	};

	explicit PtyProcessController(QObject *parent = nullptr);
	~PtyProcessController() override;

	bool start(QString const &program, QStringList const &arguments, QSize terminal_size);
	// Queues bytes for the active session.  The call never writes to a PTY on the GUI thread.
	void writeInput(QByteArray const &input);
	void stop();
	bool isRunning() const;
	State state() const;
	uint64_t sessionId() const;
	// Selects the backend for the next start; an active process keeps its backend.
	void setBackend(PtyProcessBackend backend);
	void resizeTerminal(QSize size);

signals:
	void outputReady(QByteArray const &output);
	void completed(int exit_code, QString const &error_message);
	void stateChanged(PtyProcessController::State state, uint64_t session_id);
	void inputRejected(QString const &reason);
};

#endif // PTYPROCESSCONTROLLER_H
