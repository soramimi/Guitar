#include "SettingAiForm.h"
#include "ui_SettingAiForm.h"
#include "Logger.h"
#include <QMessageBox>
#include <ai/GenerativeAI.h>
#include <common/q/helper.h>

using ApiKeyFrom = AiApiKeys::KeyFrom;

// ProviderFormData は設定画面が開いている間のみ存在する、プロバイダごとの編集バッファ。
// ApplicationSettings には exchange() が呼ばれたときにのみ読み書きするため、
// 直接設定を書き換えず、このバッファ経由で操作する。
struct SettingAiForm::ProviderFormData {
	GenerativeAI::ProviderID id = GenerativeAI::ProviderID::Unknown;
	GenerativeAI::ProviderInfo const *info = nullptr;
	ProviderFormData(GenerativeAI::ProviderID provider)
		: id(provider)
		, info(GenerativeAI::provider_info(provider))
	{
	}
	std::string env_name() const
	{
		return GenerativeAI::provider_info(id)->env_name;
	}
	bool operator == (const ProviderFormData &other) const
	{
		return id == other.id;
	}
};

struct SettingAiForm::Private {
	GenerativeAI::Model current_model_;
	GenerativeAI::ProviderID current_provider_id_ = GenerativeAI::ProviderID::Unknown;
	std::vector<SettingAiForm::ProviderFormData> provider_formdata_;
	AiApiKeys api_keys_;

	SettingAiForm::ProviderFormData *provider_formdata(GenerativeAI::ProviderID id)
	{
		for (auto &p : provider_formdata_) {
			if (p.id == id) {
				return &p;
			}
		}
		return nullptr;
	}
	GenerativeAI::ProviderID current_provider_id() const
	{
		return current_provider_id_;
	}
	void set_current_provider(GenerativeAI::ProviderID id)
	{
		current_provider_id_ = id;
	}
	SettingAiForm::ProviderFormData *current_provider()
	{
		return provider_formdata(current_provider_id_);
	}
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

	// GenerativeAI::ProviderID の定義に基づいて、利用可能なプロバイダのフォームデータを初期化する。
	for (GenerativeAI::ProviderID id : GenerativeAI::ai_provider_id_list_for_present_to_users()) {
		m->provider_formdata_.emplace_back(id);
	}

	m->set_current_provider(GenerativeAI::ProviderID::Unknown);

	for (size_t i = 0; i < m->provider_formdata_.size(); i++) {
		int id = static_cast<int>(m->provider_formdata_[i].info->id);
		QString text = QString::fromStdString(m->provider_formdata_[i].info->description);
		ui->comboBox_provider->addItem(text, QVariant(id));
	}

	QStringList list;
	{
		auto vec = GenerativeAI::ai_model_presets();
		for (auto &m : vec) {
			list.push_back((QS)m.model_uri().string);
		}
	}
	// モデル一覧を追加する際にシグナルをブロックする。
	// comboBox_ai_model への addItems で on_comboBox_ai_model_currentTextChanged が
	// 発火すると guessProviderFromModelName が呼ばれてしまうのを防ぐ。
	// comboBox_provider のブロックは addItems より後で開始するため実質無効であり、
	// 現状は comboBox_ai_model のみが保護対象となっている。
	bool b1 = ui->comboBox_ai_model->blockSignals(true);
	bool b2 = ui->comboBox_provider->blockSignals(true);
	{
		ui->comboBox_ai_model->addItems(list);
	}
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
 * @param id 検索するプロバイダID
 * @return 対応する ProviderFormData のポインタ。見つからない場合は nullptr。
 */
SettingAiForm::ProviderFormData *SettingAiForm::formdata(GenerativeAI::ProviderID id)
{
	return m->provider_formdata(id);
}
SettingAiForm::ProviderFormData const *SettingAiForm::formdata(GenerativeAI::ProviderID id) const
{
	return m->provider_formdata(id);
}

/**
 * @brief 環境変数名に対応するフォームデータを返す。
 * @param env_name 検索する環境変数名（例: "OPENAI_API_KEY"）
 * @return 対応する ProviderFormData のポインタ。見つからない場合は nullptr。
 */
SettingAiForm::ProviderFormData *SettingAiForm::formdata_by_env_name(std::string const &env_name)
{
	for (auto &ai : m->provider_formdata_) {
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
	return &m->provider_formdata_.front();
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

	GenerativeAI::ProviderID id = GenerativeAI::ProviderID::Unknown; // for debug
	Pointers conf; // 設定ファイルの値を保存するためのポインタ
	Pointers form; // 設定フォームの値を保存するためのポインタ

	ExchangePointers() = default;
	ExchangePointers(GenerativeAI::ProviderID id, Pointers conf_pts, Pointers form_pts)
		: id(id)
		, conf(conf_pts)
		, form(form_pts)
	{}
};

static bool isKeyEnvDefined(std::string const &env_name)
{
	for (GenerativeAI::ProviderInfo const &info : GenerativeAI::complete_provider_table()) {
		if (info.env_name == env_name) {
			return true;
		}
	}
	return false;
}

GenerativeAI::ModelURI SettingAiForm::currentModelURI() const
{
	return m->current_model_.model_uri();
}

AiApiKeys::Item *SettingAiForm::currentKeyItem()
{
	std::string envname;
	SettingAiForm::ProviderFormData const *p = formdata(m->current_provider_id());
	if (p) {
		envname = p->env_name();
		// if (envname.empty()) {
		// 	auto uri = currentModelURI();
		// 	envname = GenerativeAI::makeEnvName(uri);
		// }
	}
	if (!envname.empty()) {
		auto it = m->api_keys_.map.find(envname);
		if (it == m->api_keys_.map.end()) {
			// 存在しないenv_nameの場合は新規エントリを作成する。
			it = m->api_keys_.map.emplace(envname, AiApiKeys::Item{}).first;

			if (!isKeyEnvDefined(envname)) {
				// プロバイダテーブルに定義されていないenv_nameの場合はユーザー入力モードとする。
				it->second.from = AiApiKeys::KeyFrom::LocalSecret;
			}
		}
		return &it->second;
	}
	return nullptr;
}

/**
 * @brief 指定されたAIプロバイダのAPIキーの取得元を返す。
 * @param id AIプロバイダID
 * @return APIキーの取得元を示す AiApiKeys::KeyFrom の値。プロバイダが見つからない場合は Default を返す。
 */
AiApiKeys::KeyFrom SettingAiForm::keyFrom(GenerativeAI::ProviderID id) const
{
	AiApiKeys::Item *item = const_cast<SettingAiForm *>(this)->currentKeyItem();
	if (item) {
		return item->from;
	}
	return AiApiKeys::KeyFrom::Default;
}

/**
 * @brief 設定ファイルとフォームバッファの間でデータを同期する。
 * @param save true のとき: フォームバッファ → 設定ファイル (OK ボタン押下時)
 *             false のとき: 設定ファイル → フォームバッファ (設定画面を開いたとき)
 */
void SettingAiForm::exchange(bool save)
{
	ApplicationSettings *s = settings();

	if (save) { // UI -> 設定ファイル
		auto uri = currentModelURI();

		s->generate_commit_message_with_ai = ui->groupBox_generate_commit_message_by_ai->isChecked();

		s->ai_api_keys.map.clear();
		for (SettingAiForm::ProviderFormData &formdata : m->provider_formdata_) {
			std::string envname = formdata.info->env_name;
			if (envname.empty()) continue;
			// if (envname.empty()) {
			// 	envname = GenerativeAI::makeEnvName(uri);
			// }
			AiApiKeys::Item form_item;
			auto it = m->api_keys_.map.find(envname); // ensure the key exists
			if (it != m->api_keys_.map.end()) {
				form_item = it->second;
			}
			AiApiKeys::Item *conf_item = &s->ai_api_keys.map[envname];
			*conf_item = form_item;
		}

		*s->ai_model = GenerativeAI::Model(m->current_provider_id(), uri.string);
	} else { // 設定ファイル -> UI
		ui->groupBox_generate_commit_message_by_ai->setChecked(s->generate_commit_message_with_ai);

		GenerativeAI::Model const &model = *s->ai_model;

		configureModel(model);

		m->api_keys_ = s->ai_api_keys;
		for (auto &pair : m->api_keys_.map) {
			if (!isKeyEnvDefined(pair.first)) {
				// 設定ファイルに存在するが、プロバイダテーブルに定義されていないenv_nameの場合はユーザー入力モードとする。
				pair.second.from = AiApiKeys::KeyFrom::LocalSecret;
			}
		}

		SettingAiForm::ProviderFormData *provider = m->current_provider();
		if (provider) {
			setRadioButtons(true, keyFrom(model.provider_id()));
		} else {
			setRadioButtons(false, ApiKeyFrom::Default);
		}

		reflectSettingsToUI();
	}
}

/**
 * @brief APIキーの取得元を選択するラジオボタンの状態を設定する。
 * @param enabled true のとき両方のラジオボタンを有効化し、from に応じて選択状態を設定する。
 *                false のとき両方を無効化する（Unknown プロバイダ選択時など）。
 * @param from どちらのラジオボタンを選択するかを示す値。enabled が false のときは無視される。
 */
void SettingAiForm::setRadioButtons(bool enabled, AiApiKeys::KeyFrom from)
{
	bool b1 = ui->radioButton_use_environment_value->blockSignals(true);
	bool b2 = ui->radioButton_use_custom_api_key->blockSignals(true);

	if (enabled) {
		std::string envname = m->current_provider()->env_name();
		ui->radioButton_use_environment_value->setEnabled(!envname.empty()); // env_name が空のときは環境変数モードを選択できないようにする。
		if (envname.empty()) {
			from = ApiKeyFrom::LocalSecret;
		}

		ui->radioButton_use_custom_api_key->setEnabled(true);

		switch (from) {
		case ApiKeyFrom::Environment:
			ui->radioButton_use_environment_value->setChecked(true);
			break;
		case ApiKeyFrom::LocalSecret:
			ui->radioButton_use_custom_api_key->setChecked(true);
			ui->lineEdit_api_key->setEnabled(true);
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
 * ApiKeyFrom::Environment の場合は実際に環境変数を読んでテキストフィールドに表示するが、
 * 編集不可(setEnabled(false))にして読み取り専用として扱う。
 * ApiKeyFrom::LocalSecret の場合はフォームバッファの値を表示し、編集可能にする。
 */
void SettingAiForm::reflectSettingsToUI()
{
	std::string apikey;

	SettingAiForm::ProviderFormData *p = m->current_provider();
	Q_ASSERT(p);
	std::string envname = p->env_name();
	if (envname.empty() && keyFrom(p->id) == ApiKeyFrom::Environment) {
		// env_name が空のときは環境変数モードを選択できないようにする。
		ui->lineEdit_api_key->setEnabled(false);
	} else {
		char const *env = nullptr;
		switch (keyFrom(p->id)) {
		case ApiKeyFrom::Environment:
			env = std::getenv(envname.c_str());
			if (env) {
				apikey = env;
			}
			ui->lineEdit_api_key->setEnabled(false);
			break;
		case ApiKeyFrom::LocalSecret:
			if (!envname.empty()) {
				// if (envname.empty()) {
				// 	auto uri = currentModelURI();
				// 	envname = GenerativeAI::makeEnvName(uri);
				// }
				auto it = m->api_keys_.map.find(envname);
				if (it != m->api_keys_.map.end()) {
					apikey = it->second.api_key;
				}
			}
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
 * @param id 切り替え先のプロバイダのフォームデータ
 */
void SettingAiForm::changeProvider(GenerativeAI::ProviderID id)
{
	m->set_current_provider(id);

	std::string envname = m->current_provider()->env_name();
	QString text = tr("Use %1 environment value").arg(QString::fromStdString(envname));
	ui->radioButton_use_environment_value->setText(text);

	reflectSettingsToUI();
}

/**
 * @brief APIキー入力欄のテキスト変更時に呼ばれる。
 *        環境変数モードのときは何もしない。ユーザー入力モードのときのみフォームバッファに書き込む。
 */
void SettingAiForm::on_lineEdit_api_key_textChanged(const QString &arg1)
{
	if (ui->radioButton_use_custom_api_key->isChecked()) {
		auto *keyitem = currentKeyItem();
		if (keyitem) {
			keyitem->api_key = arg1.toStdString();
		}
	}
}

/**
 * @brief 「環境変数を使用する」ラジオボタンが押されたとき、APIキー取得元を Environment に切り替える。
 */
void SettingAiForm::on_radioButton_use_environment_value_clicked()
{
	auto *keyitem = currentKeyItem();
	if (keyitem) {
		keyitem->from = ApiKeyFrom::Environment;
	}
	reflectSettingsToUI();
}

/**
 * @brief 「カスタムAPIキーを使用する」ラジオボタンが押されたとき、APIキー取得元を LocalSecret に切り替える。
 */
void SettingAiForm::on_radioButton_use_custom_api_key_clicked()
{
	auto *keyitem = currentKeyItem();
	if (keyitem) {
		keyitem->from = ApiKeyFrom::LocalSecret;
	}
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
 * @brief AIプロバイダのコンボボックスの選択が変更されたとき、選択されたプロバイダに切り替える。
 *        切り替え後、APIキーの取得元を示すラジオボタンの状態を更新する。
 */
void SettingAiForm::on_comboBox_provider_currentIndexChanged(int index)
{
	GenerativeAI::ProviderID id = GenerativeAI::ProviderID::Unknown;
	if (index >= 0 && index < ui->comboBox_provider->count()) {
		QVariant v = ui->comboBox_provider->itemData(index);
		if (v.isValid()) {
			bool ok = false;
			int val = v.toInt(&ok);
			if (ok) {
				id = static_cast<GenerativeAI::ProviderID>(val);
			}
		}
	}

	changeProvider(id);

	auto *keyitem = currentKeyItem();
	if (keyitem) {
		setRadioButtons(true, keyitem->from);
	}
}

/**
 * @brief モデル名文字列からプロバイダを推定し、プロバイダのコンボボックスを更新する。
 *        推定できない場合（index < 1）はインデックス0（Unknown）にフォールバックする。
 * @param s モデル名文字列
 */
void SettingAiForm::guessProviderFromModelName(std::string const &s)
{
	m->current_model_ = GenerativeAI::Model::from_name(s);

	int data = static_cast<int>(m->current_model_.provider_id());
	int index = ui->comboBox_provider->findData(data);
	if (index < 1) {
		index = 0;
	}
	ui->comboBox_provider->setCurrentIndex(index);
}

/**
 * @brief モデル名文字列でUIを設定し、プロバイダを自動推定する。
 *        シグナルをブロックしないため、guessProviderFromModelName が連鎖的に呼ばれる。
 * @param model_uri モデル名文字列
 */
void SettingAiForm::configureModelByString(std::string const &model_uri)
{
	ui->comboBox_ai_model->setCurrentText(QString::fromStdString(model_uri));
	guessProviderFromModelName(model_uri);
}

/**
 * @brief AIモデル情報をUIに反映する。
 *        プロバイダが既知の場合はシグナルをブロックして直接設定し、
 *        不明な場合はシグナル経由でプロバイダを自動推定させる。
 * @param model 設定するAIモデル
 */
void SettingAiForm::configureModel(GenerativeAI::Model const &model)
{
	GenerativeAI::ProviderID id = model.provider_id();
	if (id == GenerativeAI::ProviderID::Unknown) {
		// モデルからプロバイダを推定できない場合は、モデル名文字列から推定させる。
		configureModelByString(model.model_uri().string);
		return;
	}

	m->current_model_ = model;

	// プロバイダが既知の場合はシグナルをブロックしてモデル名を設定した後、
	// プロバイダのコンボボックスを明示的に設定する。
	// シグナルをブロックしないと guessProviderFromModelName が二重に呼ばれてしまう。
	bool b = ui->comboBox_ai_model->blockSignals(true);
	{
		ui->comboBox_ai_model->setCurrentText((QS)model.model_uri().string);
	}
	ui->comboBox_ai_model->blockSignals(b);

	int i = ui->comboBox_provider->findData(QVariant(static_cast<int>(id)));
	ui->comboBox_provider->setCurrentIndex(i);
}

/**
 * @brief AIモデルのコンボボックスのテキストが変更されたとき、プロバイダを自動推定する。
 */
void SettingAiForm::on_comboBox_ai_model_currentTextChanged(const QString &arg1)
{
	guessProviderFromModelName(arg1.toStdString());
}

