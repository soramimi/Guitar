#ifndef MYSETTINGS_H
#define MYSETTINGS_H

#include <QSettings>

class ApplicationBasicData;

class MySettings : public QSettings {
	Q_OBJECT
public:
	explicit MySettings();
};

#endif // MYSETTINGS_H
