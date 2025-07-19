#include "Profile.h"
#include <QDebug>

void Profile::log(const QString &s)
{
	// qDebug() << s;
}

Profile::Profile(const char *func)
	: func_(func)
{
	// log(QString("--- profile [enter] %1").arg(func));
	qDebug() << "--- profile [enter]\t" << func;
	timer_.start();
}

Profile::~Profile()
{
	// log(QString("--- profile [leave] %1 @ %2ms").arg(func_).arg(timer_.elapsed()));
	qDebug() << "--- profile [leave]\t" << func_ << "\t" << timer_.elapsed();
}
