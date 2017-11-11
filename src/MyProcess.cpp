#include "MyProcess.h"

QString AbstractProcess::toQString(const std::vector<char> &vec)
{
	if (vec.empty()) return QString();
	return QString::fromUtf8(&vec[0], vec.size());
}

