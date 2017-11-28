#ifndef UNIXPTYPROCESS_H
#define UNIXPTYPROCESS_H

#include "AbstractProcess.h"
#include <QThread>

class UnixPtyProcess : public AbstractPtyProcess, public QThread {
private:
	struct Private;
	Private *m;
protected:
	void run();
public:
	UnixPtyProcess();
	~UnixPtyProcess();
	bool isRunning() const;
	void writeInput(char const *ptr, int len);
	int readOutput(char *ptr, int len);
	void start(QString const &cmd);
	bool wait(unsigned long time = ULONG_MAX);
	void stop();

	// AbstractPtyProcess interface
public:
};

#endif // UNIXPTYPROCESS_H
