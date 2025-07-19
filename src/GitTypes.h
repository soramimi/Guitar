#ifndef GITTYPES_H
#define GITTYPES_H

#include <QString>

struct GitFileItem {
	QString name;
	bool isdir = false;
};

#endif // GITTYPES_H
