#ifndef WIN32PTYPROCESS_H
#define WIN32PTYPROCESS_H

#include <QString>
#include <QThread>
#include <vector>


class Win32PtyProcess : public QThread {
private:
	struct Private;
	Private *m;

	static QString getProgram(QString const &cmdline);

	void close();
protected:
	void run();
public:
	Win32PtyProcess();
	~Win32PtyProcess();
	int readOutput(char *dstptr, int maxlen);
	void writeInput(char const *ptr, int len);
	void start(QString const &cmdline);
	void stop();
	int wait();
	std::vector<char> const *result() const;
};


#endif // WIN32PTYPROCESS_H
