#ifndef PTYPROCESSCONTROLLER_H
#define PTYPROCESSCONTROLLER_H

#include "PtyProcessFactory.h"

#include <QByteArray>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>

class PtyProcessController : public QObject {
	Q_OBJECT
private:
	struct Private;
	Private *m;
	void drainOutput();
	void handleCompleted();
public:
	explicit PtyProcessController(QObject *parent = nullptr);
	~PtyProcessController() override;

	bool start(QString const &program, QStringList const &arguments, QSize terminal_size);
	void stop();
	bool isRunning() const;
	// Selects the backend for the next start; an active process keeps its backend.
	void setBackend(PtyProcessBackend backend);
	void resizeTerminal(QSize size);

signals:
	void outputReady(QByteArray const &output);
	void completed(int exit_code, QString const &error_message);
};

#endif // PTYPROCESSCONTROLLER_H
