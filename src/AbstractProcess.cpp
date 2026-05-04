#include "AbstractProcess.h"

void AbstractPtyProcess::setChangeDir(QString const &dir)
{
	change_dir_ = dir;
}

std::string AbstractPtyProcess::getMessage() const // deprecated
{
	if (stdout_bytes_.empty()) return {};
	return std::string(&stdout_bytes_[0], stdout_bytes_.size());
}

void AbstractPtyProcess::clearMessage()
{
	output_vector_.clear();
	stdout_bytes_.clear();
}
