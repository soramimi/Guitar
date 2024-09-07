#include "AbstractProcess.h"

void AbstractPtyProcess::setChangeDir(QString const &dir)
{
	change_dir_ = dir;
}

QString AbstractPtyProcess::getMessage() const
{
	QString s;
	if (!output_vector_.empty()) {
		s = QString::fromUtf8(&output_vector_[0], output_vector_.size());
	}
	return s;
}

void AbstractPtyProcess::clearMessage()
{
	output_vector_.clear();
}
