#include "ConfigCredentialHelperDialog.h"
#include "ui_ConfigCredentialHelperDialog.h"

ConfigCredentialHelperDialog::ConfigCredentialHelperDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::ConfigCredentialHelperDialog)
{
	ui->setupUi(this);
}

ConfigCredentialHelperDialog::~ConfigCredentialHelperDialog()
{
	delete ui;
}

void ConfigCredentialHelperDialog::setHelper(QString const &helper)
{
	if (helper.isEmpty()) {
		ui->radioButton_none->setChecked(true);
		return;
	}
	if (helper == "wincred") {
		ui->radioButton_wincred->setChecked(true);
		return;
	}
	if (helper == "winstore") {
		ui->radioButton_winstore->setChecked(true);
		return;
	}
	ui->lineEdit_helper->setText(helper);
	ui->radioButton_other->setChecked(true);
}

QString ConfigCredentialHelperDialog::helper() const
{
	if (ui->radioButton_wincred->isChecked()) return "wincred";
	if (ui->radioButton_winstore->isChecked()) return "winstore";
	if (ui->radioButton_other->isChecked()) return ui->lineEdit_helper->text();
	return QString();
}


