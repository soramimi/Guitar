#include "AbstractProcess.h"

void AbstractPtyProcess::setChangeDir(QString const &dir)
{
	change_dir = dir;
}

void AbstractPtyProcess::setVariant(QVariant const &value)
{	// setUserDataという名前だとQObject::setUserDataとかぶる
	user_data = value;
}

QVariant const &AbstractPtyProcess::userVariant() const
{
	return user_data;
}

