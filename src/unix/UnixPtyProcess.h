#ifndef UNIXPTYPROCESS_H
#define UNIXPTYPROCESS_H

#include "AbstractProcess.h"
#include <QThread>

class UnixPtyProcess : public AbstractPtyProcess, public QThread {
private:
	struct Private;
	Private *m;
	bool wait_(unsigned long time = ULONG_MAX);
	void stop_();
protected:
	void run() override;
public:
	UnixPtyProcess();
	~UnixPtyProcess() override;
	bool isRunning() const override;
	void writeInput(char const *ptr, int len) override;
	int readOutput(char *ptr, int len) override;
	void start(QString const &cmd, QString const &env) override;
	bool wait(unsigned long time = ULONG_MAX) override;
	void stop() override;
	int getExitCode() const override;
	void readResult(std::vector<char> *out) override;
};

#endif // UNIXPTYPROCESS_H
