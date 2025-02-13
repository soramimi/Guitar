#ifndef PROFILE_H
#define PROFILE_H

#include <QElapsedTimer>
#include <QString>
#include <QDebug>

class Profile {
private:
	QString func_;
	QElapsedTimer timer_;
	void log(QString const &s)
	{
		qDebug() << s;
	}
public:
	Profile(const char *func)
		: func_(func)
	{
		log(QString("--- profile [enter] %1").arg(func));
		timer_.start();
	}
	~Profile()
	{
		log(QString("--- profile [leave] %1 @ %2ms").arg(func_).arg(timer_.elapsed()));
	}
};
#define PROFILE Profile _profile(Q_FUNC_INFO);

#endif // PROFILE_H
