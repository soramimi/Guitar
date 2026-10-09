#include "SettingAiForm.h"
#include "ui_SettingAiForm.h"
#include "Logger.h"
#include "SelectAiModelDialog.h"
#include <ChangePinDialog.h>
#include <QMessageBox>
#include <SecureStoreGUI.h>
#include <ai/GenerativeAI.h>
#include <common/q/helper.h>

/**
 * @brief コンストラクタ。UIを初期化し、プロバイダ一覧とAIモデルプリセット一覧をコンボボックスに追加する。
 * @param parent 親ウィジェット
 */
SettingAiForm::SettingAiForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingAiForm)
{
	ui->setupUi(this);

}

/**
 * @brief デストラクタ。
 */
SettingAiForm::~SettingAiForm()
{
	delete ui;
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

		s->generate_commit_message_with_ai = ui->groupBox_generate_commit_message_by_ai->isChecked();

	} else { // 設定ファイル -> UI

		ui->groupBox_generate_commit_message_by_ai->setChecked(s->generate_commit_message_with_ai);

	}
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

void SettingAiForm::on_pushButton_model_config_clicked()
{
	SelectAiModelDialog dlg(this);
	dlg.exec(this);
}

void SettingAiForm::lockVault()
{
	if (global->vault && global->vault.vault->isUnlocked()) {
		global->vault.vault->lock();
		global->vault.reset();
	}
}

void SettingAiForm::on_pushButton_change_pin_clicked()
{
	lockVault();
	
	ChangePinDialog dlg(this, global->vault_schema(), &global->vault);
	dlg.exec();
}

void SettingAiForm::on_pushButton_lock_vault_clicked()
{
	lockVault();
}

void SettingAiForm::on_pushButton_destroy_vault_clicked()
{
#if 0
	auto r = QMessageBox::question(this, tr("Destroy Vault"), tr("Are you sure you want to destroy the Vault? This action cannot be undone."), QMessageBox::Yes | QMessageBox::No);
	if (r != QMessageBox::Yes) {
		return;
	}
#else
	QMessageBox mb(QMessageBox::Question, tr("Destroy Vault"), tr("Are you sure you want to destroy the Vault? This action cannot be undone."), QMessageBox::Yes | QMessageBox::No, this);
	mb.setDefaultButton(QMessageBox::No);
	if (mb.exec() != QMessageBox::Yes) {
		goto done;
	}
#endif
	
	on_pushButton_lock_vault_clicked();
	{
		auto vault = global->unlockVault(this);
		if (vault) {
			vault->destroy();
			global->vault.reset();
		}
	}
	
done:;	
	ui->pushButton_destroy_vault->setEnabled(false);
	ui->checkBox_confirm_destroy_vault->setChecked(false);
}

void SettingAiForm::on_checkBox_confirm_destroy_vault_checkStateChanged(const Qt::CheckState &arg1)
{
	ui->pushButton_destroy_vault->setEnabled(arg1 == Qt::Checked);
}
