#ifndef MYSETTINGS_H
#define MYSETTINGS_H

#include <QSettings>

class MySettings : public QSettings
{
	Q_OBJECT
public:
	explicit MySettings(QObject *parent = 0);
	
signals:
	
public slots:
	
};

QString makeApplicationDataDir();

extern char const *KEY_AutoReconnect;

#endif // MYSETTINGS_H
