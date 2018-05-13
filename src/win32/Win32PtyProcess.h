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
	~Win32PtyProcess();
	bool isRunning() const;
	int readOutput(char *dstptr, int maxlen);
	void writeInput(char const *ptr, int len);
	void start(QString const &cmdline);
	bool wait(unsigned long time = ULONG_MAX);
	void stop();
	std::vector<char> const *result() const;
	int getExitCode() const;
	QString getMessage() const;
};


#endif // WIN32PTYPROCESS_H
