#ifndef UNIXPROCESS_H
#define UNIXPROCESS_H

#include <vector>
#include <string>
#include <QString>

class UnixProcess {
private:
	QByteArray outvec;
	QByteArray errvec;
public:
	int run(char const *file, char * const *argv, QByteArray *out, QByteArray *err);
	int run(QString const &command, QByteArray *out, QByteArray *err);
	QString errstring();
};

#endif // UNIXPROCESS_H
