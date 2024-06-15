#ifndef SETTINGOPTIONSFORM_H
#define SETTINGOPTIONSFORM_H

#include "AbstractSettingForm.h"
#include <QWidget>

namespace Ui {
class SettingOptionsForm;
}

class SettingOptionsForm : public AbstractSettingForm {
	Q_OBJECT
private:
	Ui::SettingOptionsForm *ui;
	void refrectSettingsToUI();
public:
	explicit SettingOptionsForm(QWidget *parent = nullptr);
	~SettingOptionsForm();
	void exchange(bool save) override;
private slots:
	void on_pushButton_edit_profiles_clicked();
	void on_checkBox_use_OPENAI_API_KEY_env_value_stateChanged(int);
};

#endif // SETTINGOPTIONSFORM_H
