#ifndef GPG_H
#define GPG_H

#include <QString>


class gpg {
public:
	struct Key {
		unsigned int len;
		char type;
		QString id;
		unsigned int year;
		unsigned int month;
		unsigned int day;
		QString name;
		QString comment;
		QString mail;
		QByteArray fingerprint;
	};

	static bool listKeys(const QString &gpg_command, QList<gpg::Key> *keys);
};

#endif // GPG_H
