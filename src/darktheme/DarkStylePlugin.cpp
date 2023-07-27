#include "DarkStyle.h"
#include "DarkStylePlugin.h"
#include "LightStyle.h"
#include "TraditionalWindowsStyleTreeControl.h"

#include <QApplication>

QStyle *DarkStylePlugin::createLightStyle()
{
	return new LightStyle();
}

QStyle *DarkStylePlugin::createDarkStyle()
{
	return new DarkStyle();
}

void DarkStylePlugin::applyDarkStyle(QApplication *app)
{
    QStyle *style = createDarkStyle();
    app->setStyle(style);
    app->setPalette(style->standardPalette());
}

void DarkStylePlugin::applyLightStyle(QApplication *app)
{
    QStyle *style = createLightStyle();
    app->setStyle(style);
#ifndef Q_OS_WIN
    app->setPalette(style->standardPalette());
#endif
}


