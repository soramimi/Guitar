#ifndef INCREMENTALSEARCHPLUGIN_H
#define INCREMENTALSEARCHPLUGIN_H

#include <QtCore/qplugin.h>
#include "IncrementalSearchInterface.h"

class IncrementalSearchPlugin : public QObject, public IncrementalSearchInterface {
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "soramimi.jp.IncrementalSearchPlugin" FILE "incrementalsearchplugin.json")
	Q_INTERFACES(IncrementalSearchInterface)
public:
	IncrementalSearch *create() override
	{
		return new IncrementalSearch();
	}
};

#endif // INCREMENTALSEARCHPLUGIN_H
