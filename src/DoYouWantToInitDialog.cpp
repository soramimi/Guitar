#include "DoYouWantToInitDialog.h"
#include "ui_DoYouWantToInitDialog.h"

DoYouWantToInitDialog::DoYouWantToInitDialog(QWidget *parent, QString const &dir) :
	QDialog(parent),
	ui(new Ui::DoYouWantToInitDialog)
{
	ui->setupUi(this);
	ui->lineEdit->setText(dir);
	ui->pushButton->setEnabled(false);
}

DoYouWantToInitDialog::~DoYouWantToInitDialog()
{
	delete ui;
}

void DoYouWantToInitDialog::on_radioButton_yes_clicked()
{
	ui->pushButton->setText(tr("Next..."));
	ui->pushButton->setEnabled(true);
}

void DoYouWantToInitDialog::on_radioButton_no_clicked()
{
	ui->pushButton->setText(tr("Cancel"));
	ui->pushButton->setEnabled(true);
}

void DoYouWantToInitDialog::on_pushButton_clicked()
{
	done(ui->radioButton_yes->isChecked() ? QDialog::Accepted : QDialog::Rejected);
}
