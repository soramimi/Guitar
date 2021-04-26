#ifndef TERMINAL_H
#define TERMINAL_H

#include <QString>

class Terminal {
public:
	static void open(QString const &dir, QString const &ssh_key);
};


#endif // TERMINAL_H
