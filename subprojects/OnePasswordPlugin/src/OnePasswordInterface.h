#ifndef ONEPASSWORDINTERFACE_H
#define ONEPASSWORDINTERFACE_H

#include <QtPlugin>
#include "OnePassword.h"

class OnePasswordInterface {
public:
	OnePasswordInterface();
	virtual ~OnePasswordInterface() {}
	virtual OnePassword *create() = 0;
};

Q_DECLARE_INTERFACE(OnePasswordInterface, "mynamespace.OnePasswordPlugin/1.0")

#endif // ONEPASSWORDINTERFACE_H
