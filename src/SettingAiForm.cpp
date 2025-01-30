#include "SettingAiForm.h"
#include "ui_SettingAiForm.h"
#include "GenerativeAI.h"

#include <QMessageBox>

struct SettingAiForm::AI {
	QString custom_api_key;
	std::string env_name;
	bool use_env_value = false;
	AI(std::string const &envname)
		: env_name(envname)
	{
	}
};

struct SettingAiForm::Private {
	GenerativeAI::Model model;
	AI ai_unknown = { std::string() };
	AI ai_openai = { "OPENAI_API_KEY" };
	AI ai_anthropic = { "ANTHROPIC_API_KEY" };
	AI ai_google = { "GOOGLE_API_KEY" };
	AI ai_deepseek = { "DEEPSEEK_API_KEY" };
	AI *current_ai = nullptr;
};

SettingAiForm::SettingAiForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingAiForm)
	, m(new Private)
{
	ui->setupUi(this);
	
	m->current_ai = &m->ai_unknown;

	QStringList list;
	{
		auto vec =GenerativeAI::available_models();
		for (auto &m : vec) {
			list.push_back(m.name);
		}
	}
	bool b = ui->groupBox_generate_commit_message_by_ai->blockSignals(true);
	ui->comboBox_ai_model->addItems(list);
	ui->groupBox_generate_commit_message_by_ai->blockSignals(b);
}

SettingAiForm::~SettingAiForm()
{
	delete m;
	delete ui;
}

void SettingAiForm::exchange(bool save)
{
	struct Item {
		bool *settings_use_api_key_env_value;
		QString *settings_api_key;
		bool *private_use_env_value;
		QString *private_custom_api_key;
		Item() = default;
		Item(bool *settings_use_env_value, QString *settings_custom_api_key, bool *private_use_env_value, QString *private_custom_api_key)
			: settings_use_api_key_env_value(settings_use_env_value)
			, settings_api_key(settings_custom_api_key)
			, private_use_env_value(private_use_env_value)
			, private_custom_api_key(private_custom_api_key)
		{
		}
	};

	ApplicationSettings *s = settings();

	std::vector<Item> items;
	items.emplace_back(&s->use_openai_api_key_environment_value, &s->openai_api_key, &m->ai_openai.use_env_value, &m->ai_openai.custom_api_key);
	items.emplace_back(&s->use_anthropic_api_key_environment_value, &s->anthropic_api_key, &m->ai_anthropic.use_env_value, &m->ai_anthropic.custom_api_key);
	items.emplace_back(&s->use_google_api_key_environment_value, &s->google_api_key, &m->ai_google.use_env_value, &m->ai_google.custom_api_key);
	items.emplace_back(&s->use_deepseek_api_key_environment_value, &s->deepseek_api_key, &m->ai_deepseek.use_env_value, &m->ai_deepseek.custom_api_key);

	if (save) {
		s->generate_commit_message_by_ai = ui->groupBox_generate_commit_message_by_ai->isChecked();

		for (size_t i = 0; i < items.size(); i++) {
			*items[i].settings_use_api_key_env_value = *items[i].private_use_env_value;
			*items[i].settings_api_key = *items[i].private_custom_api_key;
		}

		s->ai_model = ui->comboBox_ai_model->currentText();
	} else {
		ui->groupBox_generate_commit_message_by_ai->setChecked(s->generate_commit_message_by_ai);

		ui->comboBox_ai_model->setCurrentText(s->ai_model.name); // on_comboBox_ai_model_currentTextChanged() が呼ばれた先で changeAI() も呼ばれる

		for (size_t i = 0; i < items.size(); i++) {
			*items[i].private_use_env_value = *items[i].settings_use_api_key_env_value;
			*items[i].private_custom_api_key = *items[i].settings_api_key;
		}

		if (m->current_ai) {
			setRadioButtons(true, m->current_ai->use_env_value);
		} else {
			setRadioButtons(false, false);

		}
		refrectSettingsToUI();
	}
}

void SettingAiForm::setRadioButtons(bool enabled, bool use_env_value)
{
	bool b1 = ui->radioButton_use_environment_value->blockSignals(true);
	bool b2 = ui->radioButton_use_custom_api_key->blockSignals(true);

	if (enabled) {
		ui->radioButton_use_environment_value->setEnabled(true);
		ui->radioButton_use_custom_api_key->setEnabled(true);

		(use_env_value ? ui->radioButton_use_environment_value
						   : ui->radioButton_use_custom_api_key
		 )->setChecked(true);
	} else {
		ui->radioButton_use_environment_value->setEnabled(false);
		ui->radioButton_use_custom_api_key->setEnabled(false);
	}

	ui->radioButton_use_environment_value->blockSignals(b1);
	ui->radioButton_use_custom_api_key->blockSignals(b2);
}

void SettingAiForm::refrectSettingsToUI()
{
	QString apikey;
	if (m->current_ai->use_env_value) {
		apikey = getenv(m->current_ai->env_name.c_str());
		ui->lineEdit_api_key->setEnabled(false);
	} else {
		apikey = m->current_ai->custom_api_key;
		ui->lineEdit_api_key->setEnabled(true);
	}
	bool b = ui->lineEdit_api_key->blockSignals(true);
	ui->lineEdit_api_key->setText(apikey);
	ui->lineEdit_api_key->blockSignals(b);
}

void SettingAiForm::changeAI(AI *ai)
{
	m->current_ai = ai;
	QString text = tr("Use %1 environment value").arg(QString::fromStdString(m->current_ai->env_name));
	ui->radioButton_use_environment_value->setText(text);
	refrectSettingsToUI();

}

void SettingAiForm::on_lineEdit_api_key_textChanged(const QString &arg1)
{
	if (!ui->radioButton_use_environment_value->isChecked()) {
		m->current_ai->custom_api_key = arg1;
	}
}

void SettingAiForm::on_radioButton_use_environment_value_clicked()
{
	m->current_ai->use_env_value = true;
	refrectSettingsToUI();
}


void SettingAiForm::on_radioButton_use_custom_api_key_clicked()
{
	m->current_ai->use_env_value = false;
	refrectSettingsToUI();
}

void SettingAiForm::on_groupBox_generate_commit_message_by_ai_clicked(bool checked)
{
	if (checked) {
		QString msg = tr("ATTENTION");
		msg += "\n\n";
		// ja: AIを利用したコミットメッセージの生成機能を有効にすると、ローカルコンテンツの一部がクラウドサービスに送信されることに同意したものと見なされます。
		msg += tr("By enabling the commit message generation feature using AI, you are deemed to agree that part of your local content will be sent to the cloud service.");
		msg += "\n\n";
		// ja: あなたはAIモデルの選択、APIの利用料金、情報セキュリティに注意する必要があります。
		msg += tr("You should be aware of AI model selection, API usage fees, and information security.");
		if (QMessageBox::warning(this, tr("Commit Message Generation with AI"), msg, QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)	{
			ui->groupBox_generate_commit_message_by_ai->setChecked(false);
		}
	}
}

void SettingAiForm::on_comboBox_ai_model_currentTextChanged(const QString &arg1)
{
	GenerativeAI::Model model(arg1);
	GenerativeAI::Type type = model.type();
	if (m->model.type() != type) {
		AI *ai = nullptr;
		switch (type) {
		case GenerativeAI::GPT:
			ai = &m->ai_openai;
			break;
		case GenerativeAI::CLAUDE:
			ai = &m->ai_anthropic;
			break;
		case GenerativeAI::GEMINI:
			ai = &m->ai_google;
			break;
		case GenerativeAI::DEEPSEEK:
			ai = &m->ai_deepseek;
			break;
		}
		if (ai) {
			setRadioButtons(true, ai->use_env_value);
		} else {
			setRadioButtons(false, false);
			ai = &m->ai_unknown;
		}
		changeAI(ai);
		m->model = model;
	}
}




