#include "AreYouSureYouWantToContinueConnectingDialog.h"
#include "ui_AreYouSureYouWantToContinueConnectingDialog.h"

AreYouSureYouWantToContinueConnectingDialog::AreYouSureYouWantToContinueConnectingDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::AreYouSureYouWantToContinueConnectingDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);
}

AreYouSureYouWantToContinueConnectingDialog::~AreYouSureYouWantToContinueConnectingDialog()
{
	delete ui;
}

AreYouSureYouWantToContinueConnectingDialog::Result AreYouSureYouWantToContinueConnectingDialog::result() const
{
	return result_;
}

void AreYouSureYouWantToContinueConnectingDialog::setLabel(QString const &label)
{
	ui->label->setText(label);
}

void AreYouSureYouWantToContinueConnectingDialog::on_pushButton_continue_clicked()
{
	QString s = ui->lineEdit->text();
	if (s.compare("yes", Qt::CaseInsensitive) == 0) {
		result_ = Result::Yes;
		done(QDialog::Accepted);
	} else if (s.compare("no", Qt::CaseInsensitive) == 0) {
		result_ = Result::No;
		done(QDialog::Accepted);
	} else {
		ui->label_notify->setStyleSheet("QLabel { color:red; }");
		ui->label_notify->setText("Enter yes or no");
	}
}

void AreYouSureYouWantToContinueConnectingDialog::on_pushButton_close_clicked()
{
	done(QDialog::Rejected);
}

void AreYouSureYouWantToContinueConnectingDialog::reject()
{
	// ignore
}

