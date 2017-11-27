#ifndef UNIXPTYPROCESS_H
#define UNIXPTYPROCESS_H

//#include "qtermwidget/Pty.h"

#include <QThread>

//class UnixPtyProcess : public Konsole::Pty {
//public:
//	int start(const QString& program);
//};

class UnixPtyProcess2 : public QThread {
private:
	struct Private;
	Private *m;
protected:
	void run();
public:
	UnixPtyProcess2();
	~UnixPtyProcess2();
	void writeInput(char const *ptr, int len);
	int readOutput(char *ptr, int len);
	void start(QString const &cmd);
	void stop();
};


#endif // UNIXPTYPROCESS_H
