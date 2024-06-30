#include "AbstractProcess.h"

void AbstractPtyProcess::setChangeDir(QString const &dir)
{
	change_dir = dir;
}

QVariant const &AbstractPtyProcess::userVariant() const
{
	return user_data;
}

