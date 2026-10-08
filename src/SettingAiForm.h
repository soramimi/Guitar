#ifndef SETTINGAIFORM_H
#define SETTINGAIFORM_H

#include "AbstractSettingForm.h"
#include <QWidget>

namespace Ui {
class SettingAiForm;
}

class SettingAiForm : public AbstractSettingForm {
	Q_OBJECT
private:
	Ui::SettingAiForm *ui;
public:
	explicit SettingAiForm(QWidget *parent = nullptr);
	~SettingAiForm();
	void exchange(bool save) override;
private slots:
	void on_groupBox_generate_commit_message_by_ai_clicked(bool checked);
	void on_pushButton_model_config_clicked();
	// void on_pushButton_setup_pin_clicked();
	void on_pushButton_change_pin_clicked();
	void on_checkBox_confirm_destroy_vault_checkStateChanged(const Qt::CheckState &arg1);
	void on_pushButton_destroy_vault_clicked();
	void on_pushButton_lock_vault_clicked();
};

#endif // SETTINGAIFORM_H
