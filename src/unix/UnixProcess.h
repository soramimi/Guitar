#ifndef UNIXPROCESS_H
#define UNIXPROCESS_H

#include <vector>
#include <string>
#include <QString>

class UnixProcess {
private:
public:
	int run(char const *file, char * const *argv, QByteArray *outvec, QByteArray *errvec);
	int run(QString const &command, QByteArray *outvec, QByteArray *errvec);
};

#endif // UNIXPROCESS_H
