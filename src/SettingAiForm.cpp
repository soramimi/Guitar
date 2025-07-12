#include "SettingAiForm.h"
#include "ui_SettingAiForm.h"
#include "GenerativeAI.h"
#include <QMessageBox>

struct SettingAiForm::Provider {
	GenerativeAI::AI provider = GenerativeAI::AI::Unknown;
	GenerativeAI::ProviderInfo const *info = nullptr;
	QString custom_api_key;
	bool use_env_value = false;
	Provider(GenerativeAI::AI provider)
		: provider(provider)
	{
		info = GenerativeAI::provider_info(provider);
	}
	std::string env_name() const
	{
		return GenerativeAI::provider_info(provider)->env_name;
	}
	bool operator == (const Provider &other) const
	{
		return provider == other.provider;
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
		{GenerativeAI::AI::OpenAI},
		{GenerativeAI::AI::Anthropic},
		{GenerativeAI::AI::Google},
		{GenerativeAI::AI::DeepSeek},
		{GenerativeAI::AI::OpenRouter},
		{GenerativeAI::AI::Ollama},
		{GenerativeAI::AI::LMStudio}
						});

	m->current_provider = unknown_provider();

	for (size_t i = 0; i < m->providers.size(); i++) {
		int index = static_cast<int>(m->providers[i].info->provider);
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
	bool b = ui->groupBox_generate_commit_message_by_ai->blockSignals(true);
	ui->comboBox_ai_model->addItems(list);
	ui->groupBox_generate_commit_message_by_ai->blockSignals(b);
}

SettingAiForm::~SettingAiForm()
{
	delete m;
	delete ui;
}

SettingAiForm::Provider *SettingAiForm::provider(GenerativeAI::AI id)
{
	for (auto &ai : m->providers) {
		if (ai.provider == id) {
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

void SettingAiForm::exchange(bool save)
{
	ApplicationSettings *s = settings();

	std::vector<Item> items;
#define ADD_ITEM(ID) { Provider *p = provider(GenerativeAI::AI::ID); \
	items.emplace_back(&s->use_env_api_key_##ID, &s->api_key_##ID, &p->use_env_value, &p->custom_api_key); }
	ADD_ITEM(OpenAI);
	ADD_ITEM(Anthropic);
	ADD_ITEM(Google);
	ADD_ITEM(DeepSeek);
	ADD_ITEM(OpenRouter);
	// the following has no API key
	// ADD_ITEM(Ollama);
	// ADD_ITEM(LMStudio);
#undef ADD_ITEM

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

		s->ai_model = GenerativeAI::Model(m->current_provider->provider, ui->comboBox_ai_model->currentText().toStdString());
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
			setRadioButtons(true, m->current_provider->use_env_value);
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
	if (m->current_provider->use_env_value) {
		apikey = getenv(m->current_provider->env_name().c_str());
		ui->lineEdit_api_key->setEnabled(false);
	} else {
		apikey = m->current_provider->custom_api_key;
		ui->lineEdit_api_key->setEnabled(true);
	}
	bool b = ui->lineEdit_api_key->blockSignals(true);
	ui->lineEdit_api_key->setText(apikey);
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
		m->current_provider->custom_api_key = arg1;
	}
}

void SettingAiForm::on_radioButton_use_environment_value_clicked()
{
	m->current_provider->use_env_value = true;
	refrectSettingsToUI();
}


void SettingAiForm::on_radioButton_use_custom_api_key_clicked()
{
	m->current_provider->use_env_value = false;
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
	(void)index;
	int i = ui->comboBox_provider->currentData().toInt();
	if (i >= 0 && i < (int)m->providers.size()) {
		Provider *ai = &m->providers[i];
		setRadioButtons(true, ai->use_env_value);
		changeProvider(ai);
	}
}

void SettingAiForm::updateProviderComboBox(Provider *newai)
{
	for (auto &ai : m->providers) {
		if (&ai == newai) {
			int index = static_cast<int>(ai.info->provider);
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

	ui->comboBox_provider->setCurrentIndex(ui->comboBox_provider->findData(index));
}

void SettingAiForm::on_comboBox_ai_model_currentTextChanged(const QString &arg1)
{
	guessProviderFromModelName(arg1.toStdString());
}

