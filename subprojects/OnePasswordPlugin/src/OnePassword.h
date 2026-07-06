#ifndef ONEPASSWORD_H
#define ONEPASSWORD_H

#include <QObject>

class OnePassword : public QObject {
	Q_OBJECT
public:
	OnePassword();
	~OnePassword();
	
	virtual bool open();
	virtual QString getapikey(QString const &account_name, const QString &secret_ref_uri);
};

#endif // ONEPASSWORD_H
