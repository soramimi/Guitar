#ifndef SETTINGEXAMPLEFORM_H
#define SETTINGEXAMPLEFORM_H

#include "AbstractSettingForm.h"

namespace Ui {
class SettingExampleForm;
}

class SettingExampleForm : public AbstractSettingForm {
	Q_OBJECT
public:
	explicit SettingExampleForm(QWidget *parent = nullptr);
	~SettingExampleForm() override;
	void exchange(bool save) override;

private:
	Ui::SettingExampleForm *ui;
};

#endif // SETTINGEXAMPLEFORM_H
