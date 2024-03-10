#include "AbstractSettingForm.h"

AbstractSettingForm::AbstractSettingForm(QWidget *parent)
	: QWidget(parent)
{
}

MainWindow *AbstractSettingForm::mainwindow()
{
#if 0
	auto *w = qobject_cast<SettingsDialog *>(window());
	Q_ASSERT(w);
	return w->mainwindow();
#else
	return mainwindow_;
#endif
}

ApplicationSettings *AbstractSettingForm::settings()
{
#if 0
	auto *w = qobject_cast<SettingsDialog *>(window());
	Q_ASSERT(w);
	return &w->set;
#else
	return settings_;
#endif
}
