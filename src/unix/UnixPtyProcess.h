#ifndef UNIXPTYPROCESS_H
#define UNIXPTYPROCESS_H

#include "qtermwidget/Pty.h"

class UnixPtyProcess : public Konsole::Pty {
private:
	static void parseArgs(const QString &cmd, QStringList *out);
public:
	int start(const QString& program);
};

#endif // UNIXPTYPROCESS_H
