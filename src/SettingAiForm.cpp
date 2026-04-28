#include "SettingAiForm.h"
#include "ui_SettingAiForm.h"
#include "GenerativeAI.h"
#include "Logger.h"
#include "common/q/helper.h"
#include <QMessageBox>

struct SettingAiForm::Provider {
	GenerativeAI::AI aiid = GenerativeAI::AI::Unknown;
	GenerativeAI::ProviderInfo const *info = nullptr;
	std::string custom_api_key;
	ApplicationSettings::ApiKeyFrom api_key_from = ApplicationSettings::ApiKeyFrom::EnvValue;
	Provider(GenerativeAI::AI provider)
		: aiid(provider)
	{
		info = GenerativeAI::provider_info(provider);
	}
	std::string env_name() const
	{
		return GenerativeAI::provider_info(aiid)->env_name;
	}
	bool operator == (const Provider &other) const
	{
		return aiid == other.aiid;
	}
};

struct SettingAiForm::Private {
	std::vector<SettingAiForm::Provider> providers;
	SettingAiForm::Provider *current_provider = nullptr;
};

SettingAiForm::SettingAiForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingAiForm)
	, m(new Private)
{
	ui->setupUi(this);

	m->providers.insert(m->providers.end(), {
		{GenerativeAI::AI::Unknown},
		{GenerativeAI::AI::OpenAI_responses},
		{GenerativeAI::AI::OpenAI_chat_completions},
		{GenerativeAI::AI::Anthropic},
		{GenerativeAI::AI::Google},
		{GenerativeAI::AI::XAI},
		{GenerativeAI::AI::DeepSeek},
		{GenerativeAI::AI::OpenRouter},
		{GenerativeAI::AI::Ollama},
		{GenerativeAI::AI::LMStudio},
		{GenerativeAI::AI::LLAMACPP},
	});

	m->current_provider = unknown_provider();

	for (size_t i = 0; i < m->providers.size(); i++) {
		int index = static_cast<int>(m->providers[i].info->aiid);
		QString text = QString::fromStdString(m->providers[i].info->description);
		ui->comboBox_provider->addItem(text, QVariant(index));
	}

	QStringList list;
	{
		auto vec =GenerativeAI::ai_model_presets();
		for (auto &m : vec) {
			list.push_back(QString::fromStdString(m.long_name()));
		}
	}
	bool b1 = ui->comboBox_ai_model->blockSignals(true);
	bool b2 = ui->comboBox_provider->blockSignals(true);
	ui->comboBox_ai_model->addItems(list);
	ui->comboBox_ai_model->blockSignals(b1);
	ui->comboBox_provider->blockSignals(b2);
}

SettingAiForm::~SettingAiForm()
{
	delete m;
	delete ui;
}

SettingAiForm::Provider *SettingAiForm::provider(GenerativeAI::AI aiid)
{
	for (auto &ai : m->providers) {
		if (ai.aiid == aiid) {
			return &ai;
		}
	}
	return nullptr;
}

SettingAiForm::Provider *SettingAiForm::unknown_provider()
{
	return &m->providers.front();
}

struct Item {
	ApplicationSettings::ApiKeyFrom *settings_use_api_key_env_value;
	std::string *settings_api_key;
	ApplicationSettings::ApiKeyFrom *private_use_env_value;
	std::string *private_custom_api_key;
	Item() = default;
	Item(ApplicationSettings::ApiKeyFrom *settings_api_key_from, std::string *settings_custom_api_key, ApplicationSettings::ApiKeyFrom *private_use_env_value, std::string *private_custom_api_key)
		: settings_use_api_key_env_value(settings_api_key_from)
		, settings_api_key(settings_custom_api_key)
		, private_use_env_value(private_use_env_value)
		, private_custom_api_key(private_custom_api_key)
	{
	}
};

void SettingAiForm::exchange(bool save)
{
	ApplicationSettings *s = settings();

	std::vector<Item> items;
#if 0
#define ADD_ITEM(SHORT, LONG) { Provider *p = provider(GenerativeAI::AI::LONG); \
	items.emplace_back(&s->use_env_api_key_##SHORT, &s->api_key_##SHORT, &p->use_env_value, &p->custom_api_key); }
	ADD_ITEM(OpenAI, OpenAI_responses);
	ADD_ITEM(OpenAI, OpenAI_chat_completions);
	ADD_ITEM(Anthropic, Anthropic);
	ADD_ITEM(Google, Google);
	ADD_ITEM(XAI, XAI);
	ADD_ITEM(DeepSeek, DeepSeek);
	ADD_ITEM(OpenRouter, OpenRouter);
	// the following has no API key
	// ADD_ITEM(Ollama);
	// ADD_ITEM(LMStudio);
	// ADD_ITEM(LLAMACPP);
#undef ADD_ITEM
#else
	{ // wip:
		std::vector<GenerativeAI::ProviderInfo> const &infos = GenerativeAI::provider_table();
		for (GenerativeAI::ProviderInfo const &info : infos) {
			if (info.env_name.empty()) continue; // 環境変数名が空でないプロバイダーのみ対象とする
			Provider *p = provider(info.aiid);
			if (!p) continue;
			auto it = s->ai_api_keys.find(info.aiid);
			logprintf(LOG_DEFAULT, "--- provider: %d; tag: %s; desc: %s\n", static_cast<int>(info.aiid), info.tag.c_str(), info.description.c_str());
			ApplicationSettings::AiApiKey *api_key_info = &it->second;
			items.emplace_back(&api_key_info->from, &api_key_info->api_key, &p->api_key_from, &p->custom_api_key);
		}
	}
#endif

	if (save) {
		s->generate_commit_message_by_ai = ui->groupBox_generate_commit_message_by_ai->isChecked();

		for (size_t i = 0; i < items.size(); i++) {
			Item *item = &items[i];
			if (item->settings_use_api_key_env_value) {
				*item->settings_use_api_key_env_value = *item->private_use_env_value;
			}
			if (item->settings_api_key) {
				*item->settings_api_key = *item->private_custom_api_key;
			}
		}

		s->ai_model = GenerativeAI::Model(m->current_provider->aiid, ui->comboBox_ai_model->currentText().toStdString());
	} else {
		ui->groupBox_generate_commit_message_by_ai->setChecked(s->generate_commit_message_by_ai);

		configureModel(s->ai_model);

		for (size_t i = 0; i < items.size(); i++) {
			Item *item = &items[i];
			if (item->settings_use_api_key_env_value) {
				*item->private_use_env_value = *item->settings_use_api_key_env_value;
			}
			if (item->settings_api_key) {
				*item->private_custom_api_key = *item->settings_api_key;
			}
		}

		if (m->current_provider) {
			setRadioButtons(true, m->current_provider->api_key_from);
		} else {
			setRadioButtons(false, ApplicationSettings::ApiKeyFrom::Default);
		}
		refrectSettingsToUI();
	}
}

void SettingAiForm::setRadioButtons(bool enabled, ApplicationSettings::ApiKeyFrom from)
{
	bool b1 = ui->radioButton_use_environment_value->blockSignals(true);
	bool b2 = ui->radioButton_use_custom_api_key->blockSignals(true);

	if (enabled) {
		ui->radioButton_use_environment_value->setEnabled(true);
		ui->radioButton_use_custom_api_key->setEnabled(true);
#if 0
		(from ? ui->radioButton_use_environment_value
						   : ui->radioButton_use_custom_api_key
		 )->setChecked(true);
#endif
		switch (from) {
		case ApplicationSettings::ApiKeyFrom::EnvValue:
			ui->radioButton_use_environment_value->setChecked(true);
			break;
		case ApplicationSettings::ApiKeyFrom::UserInput:
			ui->radioButton_use_custom_api_key->setChecked(true);
			break;
		}
	} else {
		ui->radioButton_use_environment_value->setEnabled(false);
		ui->radioButton_use_custom_api_key->setEnabled(false);
	}

	ui->radioButton_use_environment_value->blockSignals(b1);
	ui->radioButton_use_custom_api_key->blockSignals(b2);
}

void SettingAiForm::refrectSettingsToUI()
{
	std::string apikey;
#if 0
	if (m->current_provider->api_key_from) {
		apikey = getenv(m->current_provider->env_name().c_str());
		ui->lineEdit_api_key->setEnabled(false);
	} else {
		apikey = m->current_provider->custom_api_key;
		ui->lineEdit_api_key->setEnabled(true);
	}
#endif
	SettingAiForm::Provider *p = m->current_provider;
	Q_ASSERT(p);
	if (p->env_name().empty()) {
		ui->lineEdit_api_key->setEnabled(false);
	} else {
		char const *env = nullptr;
		switch (p->api_key_from) {
		case ApplicationSettings::ApiKeyFrom::EnvValue:
			env = std::getenv(p->env_name().c_str());
			if (env) {
				apikey = env;
			}
			ui->lineEdit_api_key->setEnabled(false);
			break;
		case ApplicationSettings::ApiKeyFrom::UserInput:
			apikey = p->custom_api_key;
			ui->lineEdit_api_key->setEnabled(true);
			break;
		}
	}

	bool b = ui->lineEdit_api_key->blockSignals(true);
	ui->lineEdit_api_key->setText((QS)apikey);
	ui->lineEdit_api_key->blockSignals(b);
}

void SettingAiForm::changeProvider(Provider *ai)
{
	m->current_provider = ai;
	std::string envname = m->current_provider->env_name();
	QString text = tr("Use %1 environment value").arg(QString::fromStdString(envname));
	ui->radioButton_use_environment_value->setText(text);
	ui->radioButton_use_environment_value->setEnabled(!envname.empty());
	refrectSettingsToUI();
}

void SettingAiForm::on_lineEdit_api_key_textChanged(const QString &arg1)
{
	if (!ui->radioButton_use_environment_value->isChecked()) {
		m->current_provider->custom_api_key = arg1.toStdString();
	}
}

void SettingAiForm::on_radioButton_use_environment_value_clicked()
{
	m->current_provider->api_key_from = ApplicationSettings::ApiKeyFrom::EnvValue;
	refrectSettingsToUI();
}


void SettingAiForm::on_radioButton_use_custom_api_key_clicked()
{
	m->current_provider->api_key_from = ApplicationSettings::ApiKeyFrom::UserInput;
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

void SettingAiForm::on_comboBox_provider_currentIndexChanged(int index)
{
	if (index >= 0 && index < (int)m->providers.size()) {
		Provider *ai = &m->providers[index];
		setRadioButtons(true, ai->api_key_from);
		changeProvider(ai);
	}
}

void SettingAiForm::updateProviderComboBox(Provider *newai)
{
	for (auto &ai : m->providers) {
		if (&ai == newai) {
			int index = static_cast<int>(ai.info->aiid);
			ui->comboBox_provider->setCurrentIndex(ui->comboBox_provider->findData(index));
			break;
		}
	}
}

void SettingAiForm::guessProviderFromModelName(std::string const &s)
{
	GenerativeAI::Model model = GenerativeAI::Model::from_name(s);
	int data = static_cast<int>(model.provider_id());
	int index = ui->comboBox_provider->findData(data);
	if (index < 1) {
		index = 0;
	}
	ui->comboBox_provider->setCurrentIndex(index);
}

void SettingAiForm::configureModelByString(std::string const &s)
{
	ui->comboBox_ai_model->setCurrentText(QString::fromStdString(s));

	guessProviderFromModelName(s);
}

void SettingAiForm::configureModel(GenerativeAI::Model const &model)
{
	int index = static_cast<int>(model.provider_id());
	if (index < 1) { // 0 is unknown
		configureModelByString(model.long_name());
		return;
	}

	bool b = ui->comboBox_ai_model->blockSignals(true);
	ui->comboBox_ai_model->setCurrentText(QString::fromStdString(model.long_name()));
	ui->comboBox_ai_model->blockSignals(b);

	int i = ui->comboBox_provider->findData(index);
	ui->comboBox_provider->setCurrentIndex(i);
}

void SettingAiForm::on_comboBox_ai_model_currentTextChanged(const QString &arg1)
{
	guessProviderFromModelName(arg1.toStdString());
}

