#ifndef PROFILE_H
#define PROFILE_H

#include <QElapsedTimer>
#include <QString>

class Profile {
private:
	QString func_;
	QElapsedTimer timer_;
	void log(QString const &s);
public:
	Profile(const char *func);
	~Profile();
};
#define PROFILE Profile _profile(Q_FUNC_INFO)

#endif // PROFILE_H
