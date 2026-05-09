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

	struct ProviderFormData;

	void reflectSettingsToUI();
	void changeProvider(GenerativeAI::ProviderID id);
	void setRadioButtons(bool enabled, AiApiKeys::KeyFrom from);
	void configureModelByString(const std::string &model_uri);
	void configureModel(const GenerativeAI::Model &model);
	void guessProviderFromModelName(const std::string &s);
	SettingAiForm::ProviderFormData *formdata(GenerativeAI::ProviderID id);
	SettingAiForm::ProviderFormData const *formdata(GenerativeAI::ProviderID id) const;
	SettingAiForm::ProviderFormData *unknown_provider();
	SettingAiForm::ProviderFormData *formdata_by_env_name(const std::string &env_name);
	AiApiKeys::Item *currentKeyItem();
	AiApiKeys::KeyFrom keyFrom(GenerativeAI::ProviderID id) const;
	GenerativeAI::ModelURI currentModelURI() const;
public:
	explicit SettingAiForm(QWidget *parent = nullptr);
	~SettingAiForm();
	void exchange(bool save) override;
private slots:
	void on_groupBox_generate_commit_message_by_ai_clicked(bool checked);
	void on_lineEdit_api_key_textChanged(const QString &arg1);
	void on_radioButton_use_environment_value_clicked();
	void on_radioButton_use_custom_api_key_clicked();
	void on_comboBox_provider_currentIndexChanged(int index);
	void on_comboBox_ai_model_currentTextChanged(const QString &arg1);
};

#endif // SETTINGAIFORM_H
