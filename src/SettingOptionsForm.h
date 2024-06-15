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
	QString openai_api_key_;
	QString anthropic_api_key_;
	void refrectSettingsToUI(bool openai, bool anthropic);
public:
	explicit SettingOptionsForm(QWidget *parent = nullptr);
	~SettingOptionsForm();
	void exchange(bool save) override;
private slots:
	void on_pushButton_edit_profiles_clicked();
	void on_checkBox_use_OPENAI_API_KEY_env_value_stateChanged(int);
	void on_checkBox_use_ANTHROPIC_API_KEY_env_value_stateChanged(int);
	void on_lineEdit_openai_api_key_textChanged(const QString &arg1);
	void on_lineEdit_anthropic_api_key_textChanged(const QString &arg1);
};

#endif // SETTINGOPTIONSFORM_H
