#include "SelectGpgKeyDialog.h"
#include "SetGpgVerificationDialog.h"
#include "ui_SetGpgVerificationDialog.h"
#include "ApplicationGlobal.h"

SetGpgVerificationDialog::SetGpgVerificationDialog(QWidget *parent, QString const &repo) :
	QDialog(parent),
	ui(new Ui::SetGpgVerificationDialog)
{
	ui->setupUi(this);

	if (repo.isEmpty()) {
		ui->radioButton_repository->setEnabled(false);
	} else {
		QString text = tr("Repository");
		text += " : ";
		text += repo;
		ui->radioButton_repository->setText(text);
	}
	ui->radioButton_global->click();
}

SetGpgVerificationDialog::~SetGpgVerificationDialog()
{
	delete ui;
}

bool SetGpgVerificationDialog::isGlobalChecked() const
{
	return ui->radioButton_global->isChecked();
}

bool SetGpgVerificationDialog::isRepositoryChecked() const
{
	return ui->radioButton_repository->isChecked();
}

void SetGpgVerificationDialog::setKey_(const gpg::Key &key)
{
	ui->lineEdit_id->setText(key.id);
	ui->lineEdit_name->setText(key.name);
	ui->lineEdit_mail->setText(key.mail);
}

QString SetGpgVerificationDialog::id() const
{
	return ui->lineEdit_id->text();
}

QString SetGpgVerificationDialog::name() const
{
	return ui->lineEdit_name->text();
}

QString SetGpgVerificationDialog::mail() const
{
	return ui->lineEdit_mail->text();
}

void SetGpgVerificationDialog::on_pushButton_select_gpg_key_clicked()
{
	bool for_global = true;
	QList<gpg::Key> keys;
	gpg::listKeys(global->gpg_command, for_global, &keys);
	SelectGpgKeyDialog dlg(this, keys);
	if (dlg.exec() == QDialog::Accepted) {
		gpg::Key k = dlg.key();
		setKey_(k);
	}
}


void SetGpgVerificationDialog::on_radioButton_global_clicked()
{

}

void SetGpgVerificationDialog::on_radioButton_repository_clicked()
{

}
