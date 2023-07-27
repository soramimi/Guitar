#ifndef DARKSTYLEPLUGIN_H
#define DARKSTYLEPLUGIN_H

#include "../DarkStyleInterface.h"

#include <QObject>

class TraditionalWindowsStyleTreeControl;

class DarkStylePlugin : public QObject, public DarkStyleInterface {
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "jp.soramimi.DarkStylePlugin" FILE "darkstyleplugin.json")
	Q_INTERFACES(DarkStyleInterface)
private:
    QStyle *createLightStyle();
	QStyle *createDarkStyle();
public:
    void applyDarkStyle(QApplication *app) override;
    void applyLightStyle(QApplication *app) override;
};

#endif // DARKSTYLEPLUGIN_H
