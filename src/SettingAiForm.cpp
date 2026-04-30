#include "SettingAiForm.h"
#include "ui_SettingAiForm.h"
#include "GenerativeAI.h"
#include "Logger.h"
#include "common/q/helper.h"
#include <QMessageBox>

using ApiKeyFrom = ApplicationSettings::ApiKeyFrom;

// ProviderFormData は設定画面が開いている間のみ存在する、プロバイダごとの編集バッファ。
// ApplicationSettings には exchange() が呼ばれたときにのみ読み書きするため、
// 直接設定を書き換えず、このバッファ経由で操作する。
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
	// [copilot] current_provider は常に provider_formdata 内のいずれかの要素を指すポインタ。
	// [copilot] provider_formdata が再確保されると無効になるため、コンストラクタ以降は
	// [copilot] push_back 等で vector を拡張してはならない。
	SettingAiForm::ProviderFormData *current_provider = nullptr;
};

/**
 * @brief コンストラクタ。UIを初期化し、プロバイダ一覧とAIモデルプリセット一覧をコンボボックスに追加する。
 * @param parent 親ウィジェット
 */
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
	// モデル一覧を追加する際にシグナルをブロックする。
	// comboBox_ai_model への addItems で on_comboBox_ai_model_currentTextChanged が
	// 発火すると guessProviderFromModelName が呼ばれてしまうのを防ぐ。
	// comboBox_provider のブロックは addItems より後で開始するため実質無効であり、
	// 現状は comboBox_ai_model のみが保護対象となっている。
	bool b1 = ui->comboBox_ai_model->blockSignals(true);
	bool b2 = ui->comboBox_provider->blockSignals(true);
	ui->comboBox_ai_model->addItems(list);
	ui->comboBox_ai_model->blockSignals(b1);
	ui->comboBox_provider->blockSignals(b2);
}

/**
 * @brief デストラクタ。
 */
SettingAiForm::~SettingAiForm()
{
	delete m;
	delete ui;
}

/**
 * @brief AIプロバイダIDに対応するフォームデータを返す。
 * @param aiid 検索するプロバイダID
 * @return 対応する ProviderFormData のポインタ。見つからない場合は nullptr。
 */
SettingAiForm::ProviderFormData *SettingAiForm::formdata(GenerativeAI::AI aiid)
{
	for (auto &ai : m->provider_formdata) {
		if (ai.aiid == aiid) {
			return &ai;
		}
	}
	return nullptr;
}

/**
 * @brief 環境変数名に対応するフォームデータを返す。
 * @param env_name 検索する環境変数名（例: "OPENAI_API_KEY"）
 * @return 対応する ProviderFormData のポインタ。見つからない場合は nullptr。
 */
SettingAiForm::ProviderFormData *SettingAiForm::formdata_by_env_name(std::string const &env_name)
{
	for (auto &ai : m->provider_formdata) {
		if (ai.env_name() == env_name) {
			return &ai;
		}
	}
	return nullptr;
}

/**
 * @brief Unknown プロバイダのフォームデータを返す。
 * @return provider_formdata の先頭要素（Unknown プロバイダ）へのポインタ。
 * @note provider_formdata の先頭要素が必ず Unknown プロバイダであることを前提とする。
 *       コンストラクタの初期化リストの順序に依存しているため、変更時は注意。
 */
SettingAiForm::ProviderFormData *SettingAiForm::unknown_provider()
{
	return &m->provider_formdata.front();
}

// ExchangePointers は、設定ファイル側(conf)とフォームバッファ側(form)の
// 対応するメンバへのポインタをペアで保持する構造体。
// exchange() でコピー方向(save/load)を切り替えるだけで双方向の同期を実現するために使う。
// ProviderFormData と ApplicationSettings::AiApiKey を env_name でマッチングして
// 事前にポインタペアを構築し、その後ループで一括コピーする設計になっている。
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

/**
 * @brief 設定ファイルとフォームバッファの間でデータを同期する。
 * @param save true のとき: フォームバッファ → 設定ファイル (OK ボタン押下時)
 *             false のとき: 設定ファイル → フォームバッファ (設定画面を開いたとき)
 */
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

		// [copilot] configureModel は内部で changeProvider → reflectSettingsToUI を呼ぶことがあるが、
		// [copilot] pointers は formdata へのポインタを保持しているため、configureModel の呼び出しで
		// [copilot] formdata の内容が変わっても、後続のループで正しく上書きされる。
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
		// [copilot] configureModel 内でも reflectSettingsToUI が呼ばれる場合があるが、
		// [copilot] 上のループで formdata の api_key / from を上書きした後に改めて
		// [copilot] 呼ぶことで、設定ファイルの値を確実にウィジェットへ反映させている。
		reflectSettingsToUI();
	}
}

/**
 * @brief APIキーの取得元を選択するラジオボタンの状態を設定する。
 * @param enabled true のとき両方のラジオボタンを有効化し、from に応じて選択状態を設定する。
 *                false のとき両方を無効化する（Unknown プロバイダ選択時など）。
 * @param from どちらのラジオボタンを選択するかを示す値。enabled が false のときは無視される。
 */
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

/**
 * @brief 現在選択中のプロバイダの状態をウィジェットに反映する。
 *
 * ApiKeyFrom::EnvValue の場合は実際に環境変数を読んでテキストフィールドに表示するが、
 * 編集不可(setEnabled(false))にして読み取り専用として扱う。
 * ApiKeyFrom::UserInput の場合はフォームバッファの値を表示し、編集可能にする。
 */
void SettingAiForm::reflectSettingsToUI()
{
	std::string apikey;

	SettingAiForm::ProviderFormData *p = m->current_provider;
	Q_ASSERT(p);
	if (p->env_name().empty()) {
		// env_name が空 = Unknown プロバイダ。API キー入力欄自体を無効化する。
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

	// テキスト変更シグナルをブロックして setText する。
	// ブロックしないと on_lineEdit_api_key_textChanged が呼ばれ、
	// フォームバッファを上書きしてしまう。
	bool b = ui->lineEdit_api_key->blockSignals(true);
	ui->lineEdit_api_key->setText((QS)apikey);
	ui->lineEdit_api_key->blockSignals(b);
}

/**
 * @brief 選択中プロバイダを切り替え、UIを更新する。
 * @param ai 切り替え先のプロバイダのフォームデータ
 */
void SettingAiForm::changeProvider(ProviderFormData *ai)
{
	m->current_provider = ai;
	std::string envname = m->current_provider->env_name();
	QString text = tr("Use %1 environment value").arg(QString::fromStdString(envname));
	ui->radioButton_use_environment_value->setText(text);
	ui->radioButton_use_environment_value->setEnabled(!envname.empty());
	reflectSettingsToUI();
}

/**
 * @brief APIキー入力欄のテキスト変更時に呼ばれる。
 *        環境変数モードのときは何もしない。ユーザー入力モードのときのみフォームバッファに書き込む。
 */
void SettingAiForm::on_lineEdit_api_key_textChanged(const QString &arg1)
{
	// [copilot] radioButton_use_custom_api_key->isChecked() の肯定ではなく
	// [copilot] radioButton_use_environment_value->isChecked() の否定で判定している。
	// [copilot] 将来 EnvValue でも UserInput でもない第3の選択肢が追加された場合、
	// [copilot] この条件では意図せずバッファを書き換えてしまう点に注意。
	if (!ui->radioButton_use_environment_value->isChecked()) {
		m->current_provider->api_key = arg1.toStdString();
	}
}

/**
 * @brief 「環境変数を使用する」ラジオボタンが押されたとき、APIキー取得元を EnvValue に切り替える。
 */
void SettingAiForm::on_radioButton_use_environment_value_clicked()
{
	m->current_provider->from = ApiKeyFrom::EnvValue;
	reflectSettingsToUI();
}


/**
 * @brief 「カスタムAPIキーを使用する」ラジオボタンが押されたとき、APIキー取得元を UserInput に切り替える。
 */
void SettingAiForm::on_radioButton_use_custom_api_key_clicked()
{
	m->current_provider->from = ApiKeyFrom::UserInput;
	reflectSettingsToUI();
}

/**
 * @brief AIによるコミットメッセージ生成のグループボックスがクリックされたとき。
 *        有効化しようとした場合、クラウドへのデータ送信を警告するダイアログを表示する。
 *        ユーザーがキャンセルすると有効化を取り消す。
 */
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

/**
 * @brief プロバイダのコンボボックスの選択が変更されたとき。
 */
void SettingAiForm::on_comboBox_provider_currentIndexChanged(int index)
{
	// comboBox_provider の表示インデックスと provider_formdata の配列インデックスは
	// コンストラクタで同一順序で追加されているため一致する。
	if (index >= 0 && index < (int)m->provider_formdata.size()) {
		ProviderFormData *ai = &m->provider_formdata[index];
		setRadioButtons(true, ai->from);
		changeProvider(ai);
	}
}

/**
 * @brief 指定したプロバイダに対応するコンボボックスの選択肢を選択状態にする。
 * @param newai 選択したいプロバイダのフォームデータ
 */
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

/**
 * @brief モデル名文字列からプロバイダを推定し、プロバイダのコンボボックスを更新する。
 *        推定できない場合（index < 1）はインデックス0（Unknown）にフォールバックする。
 * @param s モデル名文字列
 */
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

/**
 * @brief モデル名文字列でUIを設定し、プロバイダを自動推定する。
 *        シグナルをブロックしないため、guessProviderFromModelName が連鎖的に呼ばれる。
 * @param s モデル名文字列
 */
void SettingAiForm::configureModelByString(std::string const &s)
{
	ui->comboBox_ai_model->setCurrentText(QString::fromStdString(s));

	guessProviderFromModelName(s);
}

/**
 * @brief AIモデル情報をUIに反映する。
 *        プロバイダが既知の場合はシグナルをブロックして直接設定し、
 *        不明な場合はシグナル経由でプロバイダを自動推定させる。
 * @param model 設定するAIモデル
 */
void SettingAiForm::configureModel(GenerativeAI::Model const &model)
{
	int index = static_cast<int>(model.provider_id());
	if (index < 1) { // 0 is unknown
		// プロバイダが不明な場合はモデル名文字列からプロバイダを自動推定させるため、
		// シグナルをブロックせずに configureModelByString() へ委譲する。
		// これにより on_comboBox_ai_model_currentTextChanged → guessProviderFromModelName
		// が発火し、プロバイダのコンボボックスが自動的に更新される。
		configureModelByString(model.long_name());
		return;
	}

	// プロバイダが既知の場合はシグナルをブロックしてモデル名を設定した後、
	// プロバイダのコンボボックスを明示的に設定する。
	// シグナルをブロックしないと guessProviderFromModelName が二重に呼ばれてしまう。
	bool b = ui->comboBox_ai_model->blockSignals(true);
	ui->comboBox_ai_model->setCurrentText(QString::fromStdString(model.long_name()));
	ui->comboBox_ai_model->blockSignals(b);

	int i = ui->comboBox_provider->findData(index);
	ui->comboBox_provider->setCurrentIndex(i);
}

/**
 * @brief AIモデルのコンボボックスのテキストが変更されたとき、プロバイダを自動推定する。
 */
void SettingAiForm::on_comboBox_ai_model_currentTextChanged(const QString &arg1)
{
	guessProviderFromModelName(arg1.toStdString());
}

