#ifndef UNIXPTYPROCESS_H
#define UNIXPTYPROCESS_H

#include "AbstractProcess.h"
#include <QThread>

class UnixPtyProcess : public AbstractPtyProcess, public QThread {
private:
	struct Private;
	Private *m;
protected:
	void run() override;
public:
	UnixPtyProcess();
	~UnixPtyProcess() override;
	bool isRunning() const override;
	void writeInput(char const *ptr, int len) override;
	int readOutput(char *ptr, int len) override;
	void start(QString const &cmd, QVariant const &userdata) override;
	bool wait(unsigned long time = ULONG_MAX) override;
	void stop() override;
	int getExitCode() const override;
	QString getMessage() const override;
	void readResult(std::vector<char> *out);
};

#endif // UNIXPTYPROCESS_H
