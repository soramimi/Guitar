#ifndef WIN32PTYPROCESS_H
#define WIN32PTYPROCESS_H

#include <AbstractProcess.h>
#include <QString>
#include <QThread>
#include <vector>


class Win32PtyProcess : public AbstractPtyProcess, public QThread {
private:
	struct Private;
	Private *m;

	static QString getProgram(QString const &cmdline);

protected:
	void run();
public:
	Win32PtyProcess();
	~Win32PtyProcess() override;
	bool isRunning() const override;
	int readOutput(char *dstptr, int maxlen) override;
	void writeInput(char const *ptr, int len) override;
	void start(QString const &cmdline, QVariant const &userdata) override;
	bool wait(unsigned long time = ULONG_MAX) override;
	void stop() override;
	int getExitCode() const override;
	QString getMessage() const override;
	void clearResult();
	void readResult(std::vector<char> *out) override;
};


#endif // WIN32PTYPROCESS_H
