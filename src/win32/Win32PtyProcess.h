#ifndef WIN32PTYPROCESS_H
#define WIN32PTYPROCESS_H

#include "../AbstractProcess.h"
#include <QString>
#include <QThread>

class Win32PtyProcess : public AbstractPtyProcess, public QThread {
private:
	struct Private;
	Private *m;
protected:
	void run();
public:
	Win32PtyProcess();
	~Win32PtyProcess() override;
	bool isRunning() const override;
	int readOutputStreaming(char *dstptr, int maxlen) override;
	void writeInput(char const *ptr, int len) override;
	void start(std::string const &cmdline, std::string const &env, bool use_input) override;
	bool wait(unsigned long time = ULONG_MAX) override;
	void stop() override;
	int getExitCode() const override;
};

#endif // WIN32PTYPROCESS_H
