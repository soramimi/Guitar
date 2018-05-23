#ifndef GPG_H
#define GPG_H

#include <QDateTime>
#include <QString>


class gpg {
public:
	struct Data {
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

	static bool listKeys(const QString &gpg_command, QList<gpg::Data> *keys);






public:
	struct Key {
		QString id;
		QDateTime timestamp;
	};

	struct UID {
		QString name;
		QString email;
		QString comment;
	};

	struct Item {
		Key pub;
		QList<Key> sub;
		QList<UID> uid;
	};

	static QList<Item> load(QByteArray const &json);

};

#endif // GPG_H
