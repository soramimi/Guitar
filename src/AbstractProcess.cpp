#include "AbstractProcess.h"
#include "ApplicationGlobal.h"
#include "TraceEventWriter.h"

void AbstractPtyProcess::setChangeDir(QString const &dir)
{
	change_dir_ = dir;
}

std::string AbstractPtyProcess::getMessage() const
{
	std::string s;
	if (!output_vector_.empty()) {
		s = std::string(&output_vector_[0], output_vector_.size());
	}
	return s;
}

void AbstractPtyProcess::clearMessage()
{
	output_vector_.clear();
}
