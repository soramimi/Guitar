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

	struct Private;
	Private *m;

	struct AI;

	void refrectSettingsToUI();
	void changeAI(AI *ai);
	void setRadioButtons(bool enabled, bool use_env_value);
	void configureModelByString(const std::string &s);
	void configureModel(const GenerativeAI::Model &model);
	SettingAiForm::AI *ai_from_provider(const GenerativeAI::Provider &provider);
	void updateProviderComboBox(AI *newai);
	void guessProviderFromModelName(const std::string &s);
public:
	explicit SettingAiForm(QWidget *parent = nullptr);
	~SettingAiForm();
	void exchange(bool save) override;
private slots:
	void on_groupBox_generate_commit_message_by_ai_clicked(bool checked);
	// void on_comboBox_ai_model_currentTextChanged(const QString &arg1);
	void on_lineEdit_api_key_textChanged(const QString &arg1);
	void on_radioButton_use_environment_value_clicked();
	void on_radioButton_use_custom_api_key_clicked();
	void on_comboBox_provider_currentIndexChanged(int index);
	void on_comboBox_ai_model_currentTextChanged(const QString &arg1);
};

#endif // SETTINGAIFORM_H
