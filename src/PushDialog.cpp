#include "PushDialog.h"
#include "ui_PushDialog.h"

PushDialog::PushDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::PushDialog)
{
	ui->setupUi(this);
}

PushDialog::~PushDialog()
{
	delete ui;
}

void PushDialog::setURL(QString const &url)
{
	int i = url.indexOf("://");
	if (i > 0) {
		int j = url.indexOf(':', i + 3);
		int k = url.indexOf('@');
		if (i < j && j < k) {
			QString s;
			s = url.mid(0, i + 3) + url.mid(k + 1);
			ui->lineEdit_url->setText(s);
			s = url.mid(i + 3, j - i - 3);
			ui->lineEdit_username->setText(s);
			s = url.mid(j + 1, k - j - 1);
			ui->lineEdit_password->setText(s);
		}
	}
}

void PushDialog::makeCompleteURL()
{
	QString s = ui->lineEdit_url->text();
	int i = s.indexOf("://");
	if (i > 0) {
		s = s.mid(0, i + 3) + ui->lineEdit_username->text() + ':' + ui->lineEdit_password->text() + '@' + s.mid(i + 3);
		ui->lineEdit_complete_url->setText(s);
	}

}

void PushDialog::on_lineEdit_url_textChanged(const QString & /*arg1*/)
{
	makeCompleteURL();
}

void PushDialog::on_lineEdit_username_textChanged(const QString & /*arg1*/)
{
	makeCompleteURL();
}

void PushDialog::on_lineEdit_password_cursorPositionChanged(int /*arg1*/, int /*arg2*/)
{
	makeCompleteURL();
}
