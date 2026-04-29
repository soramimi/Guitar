#include "SettingAiForm.h"
#include "ui_SettingAiForm.h"
#include "GenerativeAI.h"
#include "Logger.h"
#include "common/q/helper.h"
#include <QMessageBox>

using ApiKeyFrom = ApplicationSettings::ApiKeyFrom;

struct SettingAiForm::ProviderFormData {
	GenerativeAI::AI aiid = GenerativeAI::AI::Unknown;
	GenerativeAI::ProviderInfo const *info = nullptr;
	std::string api_key;
	ApiKeyFrom from = ApiKeyFrom::EnvValue;
	ProviderFormData(GenerativeAI::AI provider)
		: aiid(provider)
	{
		info = GenerativeAI::provider_info(provider);
	}
	std::string env_name() const
	{
		return GenerativeAI::provider_info(aiid)->env_name;
	}
	bool operator == (const ProviderFormData &other) const
	{
		return aiid == other.aiid;
	}
};

struct SettingAiForm::Private {
	std::vector<SettingAiForm::ProviderFormData> provider_formdata;
	SettingAiForm::ProviderFormData *current_provider = nullptr;
};

SettingAiForm::SettingAiForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingAiForm)
	, m(new Private)
{
	ui->setupUi(this);

	m->provider_formdata.insert(m->provider_formdata.end(), {
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

	for (size_t i = 0; i < m->provider_formdata.size(); i++) {
		int index = static_cast<int>(m->provider_formdata[i].info->aiid);
		QString text = QString::fromStdString(m->provider_formdata[i].info->description);
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

SettingAiForm::ProviderFormData *SettingAiForm::formdata(GenerativeAI::AI aiid)
{
	for (auto &ai : m->provider_formdata) {
		if (ai.aiid == aiid) {
			return &ai;
		}
	}
	return nullptr;
}

SettingAiForm::ProviderFormData *SettingAiForm::formdata_by_env_name(std::string const &env_name)
{
	for (auto &ai : m->provider_formdata) {
		if (ai.env_name() == env_name) {
			return &ai;
		}
	}
	return nullptr;
}

SettingAiForm::ProviderFormData *SettingAiForm::unknown_provider()
{
	return &m->provider_formdata.front();
}

struct ExchangePointers {
	struct Pointers {
		ApiKeyFrom *from = nullptr;
		std::string *api_key = nullptr;

		Pointers() = default;
		Pointers(ApiKeyFrom *from, std::string *api_key)
			: from(from)
			, api_key(api_key)
		{}
	};

	GenerativeAI::AI aiid = GenerativeAI::AI::Unknown; // for debug
	Pointers conf; // 設定ファイルの値を保存するためのポインタ
	Pointers form; // 設定フォームの値を保存するためのポインタ

	ExchangePointers() = default;
	ExchangePointers(GenerativeAI::AI aiid, Pointers conf_pts, Pointers form_pts)
		: aiid(aiid)
		, conf(conf_pts)
		, form(form_pts)
	{}
};

void SettingAiForm::exchange(bool save)
{
	ApplicationSettings *s = settings();

	std::vector<ExchangePointers> pointers; // AIプロバイダごとに、設定ファイルと設定フォームの値を交換するためのポインタのリスト
	{
		std::vector<GenerativeAI::ProviderInfo> const &infos = GenerativeAI::complete_provider_table();
		for (GenerativeAI::ProviderInfo const &info : infos) {
			if (info.symbol.empty()) continue;
			Q_ASSERT(!info.env_name.empty());
			ProviderFormData *formdata = formdata_by_env_name(info.env_name);
			if (!formdata) continue;
			auto it = s->ai_api_keys.find(info.env_name);
			if (it == s->ai_api_keys.end()) continue;
			ApplicationSettings::AiApiKey *confdata = &it->second;
			ExchangePointers::Pointers conf_pts{&confdata->from, &confdata->api_key}; // 設定ファイルの値を保存するためのポインタ
			ExchangePointers::Pointers form_pts{&formdata->from, &formdata->api_key}; // 設定フォームの値を保存するためのポインタ
			pointers.emplace_back(info.aiid, conf_pts, form_pts);
		}
	}

	if (save) { // UI -> 設定ファイル
		s->generate_commit_message_by_ai = ui->groupBox_generate_commit_message_by_ai->isChecked();

		// 設定フォームの値を設定ファイルの値に反映
		for (ExchangePointers &item : pointers) {
			ExchangePointers::Pointers *dst = &item.conf;
			ExchangePointers::Pointers const *src = &item.form;
			*dst->from = *src->from;
			*dst->api_key = misc::trimmed(*src->api_key);
		}

		s->ai_model = GenerativeAI::Model(m->current_provider->aiid, ui->comboBox_ai_model->currentText().toStdString());
	} else { // 設定ファイル -> UI
		ui->groupBox_generate_commit_message_by_ai->setChecked(s->generate_commit_message_by_ai);

		configureModel(s->ai_model);

		// 設定ファイルの値を設定フォームの値に反映
		for (ExchangePointers &item : pointers) {

			*item.form.from = *item.conf.from;
			*item.form.api_key = misc::trimmed(*item.conf.api_key);
		}

		if (m->current_provider) {
			setRadioButtons(true, m->current_provider->from);
		} else {
			setRadioButtons(false, ApiKeyFrom::Default);
		}
		refrectSettingsToUI();
	}
}

void SettingAiForm::setRadioButtons(bool enabled, ApiKeyFrom from)
{
	bool b1 = ui->radioButton_use_environment_value->blockSignals(true);
	bool b2 = ui->radioButton_use_custom_api_key->blockSignals(true);

	if (enabled) {
		ui->radioButton_use_environment_value->setEnabled(true);
		ui->radioButton_use_custom_api_key->setEnabled(true);

		switch (from) {
		case ApiKeyFrom::EnvValue:
			ui->radioButton_use_environment_value->setChecked(true);
			break;
		case ApiKeyFrom::UserInput:
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

	SettingAiForm::ProviderFormData *p = m->current_provider;
	Q_ASSERT(p);
	if (p->env_name().empty()) {
		ui->lineEdit_api_key->setEnabled(false);
	} else {
		char const *env = nullptr;
		switch (p->from) {
		case ApiKeyFrom::EnvValue:
			env = std::getenv(p->env_name().c_str());
			if (env) {
				apikey = env;
			}
			ui->lineEdit_api_key->setEnabled(false);
			break;
		case ApiKeyFrom::UserInput:
			apikey = p->api_key;
			ui->lineEdit_api_key->setEnabled(true);
			break;
		}
	}

	bool b = ui->lineEdit_api_key->blockSignals(true);
	ui->lineEdit_api_key->setText((QS)apikey);
	ui->lineEdit_api_key->blockSignals(b);
}

void SettingAiForm::changeProvider(ProviderFormData *ai)
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
		m->current_provider->api_key = arg1.toStdString();
	}
}

void SettingAiForm::on_radioButton_use_environment_value_clicked()
{
	m->current_provider->from = ApiKeyFrom::EnvValue;
	refrectSettingsToUI();
}


void SettingAiForm::on_radioButton_use_custom_api_key_clicked()
{
	m->current_provider->from = ApiKeyFrom::UserInput;
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
	if (index >= 0 && index < (int)m->provider_formdata.size()) {
		ProviderFormData *ai = &m->provider_formdata[index];
		setRadioButtons(true, ai->from);
		changeProvider(ai);
	}
}

void SettingAiForm::updateProviderComboBox(ProviderFormData *newai)
{
	for (auto &ai : m->provider_formdata) {
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

