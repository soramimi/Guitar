#ifndef ONEPASSWORDPLUGIN_H
#define ONEPASSWORDPLUGIN_H

#include <QtCore/qplugin.h>
#include "OnePasswordInterface.h"

class OnePasswordPlugin : public QObject, public OnePasswordInterface {
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "mynamespace.OnePasswordPlugin" FILE "onepasswordplugin.json")
	Q_INTERFACES(OnePasswordInterface)
public:
	OnePassword *create() override
	{
		return new OnePassword();
	}
};

#endif // ONEPASSWORDPLUGIN_H
