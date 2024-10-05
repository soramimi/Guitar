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
	GenerativeAI::Model model_;
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
	void on_lineEdit_openai_api_key_textChanged(const QString &arg1);
	void on_lineEdit_anthropic_api_key_textChanged(const QString &arg1);
	void on_lineEdit_google_api_key_textChanged(const QString &arg1);
	void on_groupBox_generate_commit_message_by_ai_clicked(bool checked);
	void on_radioButton_use_OPENAI_API_KEY_env_value_clicked();
	void on_radioButton_use_custom_openai_api_key_clicked();
	void on_radioButton_use_ANTHROPIC_API_KEY_env_value_clicked();
	void on_radioButton_use_custom_anthropic_api_key_clicked();
	void on_radioButton_use_GOOGLE_API_KEY_env_value_clicked();
	void on_radioButton_use_custom_google_api_key_clicked();
	void on_comboBox_ai_model_currentTextChanged(const QString &arg1);
};

#endif // SETTINGAIFORM_H
