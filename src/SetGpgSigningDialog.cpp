#include "SelectGpgKeyDialog.h"
#include "SetGpgSigningDialog.h"
#include "ui_SetGpgSigningDialog.h"
#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include "ConfigSigningDialog.h"

struct SetGpgSigningDialog::Private {
	QList<gpg::Data> keys;
	QString global_key_id;
	QString repository_key_id;
};

SetGpgSigningDialog::SetGpgSigningDialog(QWidget *parent, QString const &repo, const QString &global_key_id, const QString &repository_key_id)
	: QDialog(parent)
	, ui(new Ui::SetGpgSigningDialog)
	, m(new Private)
{
	ui->setupUi(this);

	gpg::listKeys(global->gpg_command, &m->keys);

	m->global_key_id = global_key_id;
	m->repository_key_id = repository_key_id;

	if (repo.isEmpty()) {
		ui->radioButton_repository->setEnabled(false);
	} else {
		QString text = tr("Repository");
		text += " : ";
		text += repo;
		ui->radioButton_repository->setText(text);
	}

	QRadioButton *w = repository_key_id.isEmpty() ? ui->radioButton_global : ui->radioButton_repository;
	w->setFocus();
	w->click();
}

SetGpgSigningDialog::~SetGpgSigningDialog()
{
	delete m;
	delete ui;
}

MainWindow *SetGpgSigningDialog::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
}

bool SetGpgSigningDialog::isGlobalChecked() const
{
	return ui->radioButton_global->isChecked();
}

bool SetGpgSigningDialog::isRepositoryChecked() const
{
	return ui->radioButton_repository->isChecked();
}

void SetGpgSigningDialog::setKey_(const gpg::Data &key)
{
	if (isGlobalChecked())     m->global_key_id = key.id;
	if (isRepositoryChecked()) m->repository_key_id = key.id;

	ui->lineEdit_id->setText(key.id);
	ui->lineEdit_name->setText(key.name);
	ui->lineEdit_mail->setText(key.mail);
}

void SetGpgSigningDialog::setKey_(QString const &key_id)
{
	gpg::Data key;
	for (gpg::Data const &k : m->keys) {
		if (k.id.compare(key_id, Qt::CaseInsensitive) == 0) {
			key = k;
			break;
		}
	}
	setKey_(key);
}

QString SetGpgSigningDialog::id() const
{
	return ui->lineEdit_id->text();
}

QString SetGpgSigningDialog::name() const
{
	return ui->lineEdit_name->text();
}

QString SetGpgSigningDialog::mail() const
{
	return ui->lineEdit_mail->text();
}

void SetGpgSigningDialog::on_pushButton_select_clicked()
{
	gpg::listKeys(global->gpg_command, &m->keys);
	SelectGpgKeyDialog dlg(this, m->keys);
	if (dlg.exec() == QDialog::Accepted) {
		gpg::Data key = dlg.key();
		for (gpg::Data const &k : m->keys) {
			if (k.id.compare(key.id, Qt::CaseInsensitive) == 0) {
				key = k;
				break;
			}
		}
		setKey_(key);
	}
}

void SetGpgSigningDialog::on_pushButton_clear_clicked()
{
	setKey_(gpg::Data());
}

void SetGpgSigningDialog::on_radioButton_global_clicked()
{
	setKey_(m->global_key_id);
}

void SetGpgSigningDialog::on_radioButton_repository_clicked()
{
	setKey_(m->repository_key_id);
}



void SetGpgSigningDialog::on_pushButton_configure_clicked()
{
	ConfigSigningDialog dlg(this, mainwindow(), true);
	dlg.exec();
}
