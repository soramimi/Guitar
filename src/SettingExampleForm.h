#ifndef SETTINGEXAMPLEFORM_H
#define SETTINGEXAMPLEFORM_H

#include "AbstractSettingForm.h"

#include <QWidget>

namespace Ui {
class SettingExampleForm;
}

class SettingExampleForm : public AbstractSettingForm
{
	Q_OBJECT

public:
	explicit SettingExampleForm(QWidget *parent = 0);
	~SettingExampleForm();
	void exchange(bool save);

private:
	Ui::SettingExampleForm *ui;
};

#endif // SETTINGEXAMPLEFORM_H
