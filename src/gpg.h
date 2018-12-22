#ifndef GPG_H
#define GPG_H

#include <QDateTime>
#include <QString>

class gpg {
public:
	struct Data {
		QString id;
		unsigned int year = 0;
		unsigned int month = 0;
		unsigned int day = 0;
		QString name;
		QString comment;
		QString mail;
		QByteArray fingerprint;
	};

	static void parse(char const *begin, char const *end, QList<gpg::Data> *keys);
	static bool listKeys(QString const &gpg_command, QList<gpg::Data> *keys);
};

#endif // GPG_H
