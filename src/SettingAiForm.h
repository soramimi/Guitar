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
	QString openai_api_key_;
	QString anthropic_api_key_;
	QString google_api_key_;
	void refrectSettingsToUI(bool openai, bool anthropic);
	void refrectSettingsToUI_openai();
	void refrectSettingsToUI_anthropic();
	void refrectSettingsToUI_google();
public:
	explicit SettingAiForm(QWidget *parent = nullptr);
	~SettingAiForm();
	void exchange(bool save) override;
private slots:
	void on_checkBox_use_OPENAI_API_KEY_env_value_stateChanged(int);
	void on_checkBox_use_ANTHROPIC_API_KEY_env_value_stateChanged(int);
	void on_lineEdit_openai_api_key_textChanged(const QString &arg1);
	void on_lineEdit_anthropic_api_key_textChanged(const QString &arg1);
	void on_lineEdit_google_api_key_textChanged(const QString &arg1);
	void on_checkBox_use_GOOGLE_API_KEY_env_value_stateChanged(int arg1);
};

#endif // SETTINGAIFORM_H
