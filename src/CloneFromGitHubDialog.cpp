#include "CloneFromGitHubDialog.h"
#include "ui_CloneFromGitHubDialog.h"

#include <QDesktopServices>
#include <QUrl>

CloneFromGitHubDialog::CloneFromGitHubDialog(QWidget *parent, const QString &username, const QString &reponame) :
	QDialog(parent),
	ui(new Ui::CloneFromGitHubDialog)
{
	ui->setupUi(this);

	QString url;

	url = QString("https://github.com/%1/%2").arg(username).arg(reponame);
	ui->label_hyperlink->setText(url);

	url = QString("git@github.com:%1/%2.git").arg(username).arg(reponame);
	ui->lineEdit_ssh->setText(url);

	url = QString("https://github.com/%1/%2.git").arg(username).arg(reponame);
	ui->lineEdit_http->setText(url);

	connect(ui->label_hyperlink, &HyperLinkLabel::clicked, this, &CloneFromGitHubDialog::onHyperlinkClicked);
	ui->pushButton_ok->setEnabled(false);
}

CloneFromGitHubDialog::~CloneFromGitHubDialog()
{
	delete ui;
}

QString CloneFromGitHubDialog::url() const
{
	return url_;
}

void CloneFromGitHubDialog::onHyperlinkClicked()
{
	QString url = ui->label_hyperlink->text();
	QDesktopServices::openUrl(QUrl(url));
}

void CloneFromGitHubDialog::updateUI()
{
	auto Check = [&](){
		if (ui->radioButton_ssh->isChecked()) {
			url_ = ui->lineEdit_ssh->text();
			if (!url_.isEmpty()) {
				return true;
			}
		}
		if (ui->radioButton_http->isChecked()) {
			url_ = ui->lineEdit_http->text();
			if (!url_.isEmpty()) {
				return true;
			}
		}
		url_ = QString();
		return false;
	};
	ui->pushButton_ok->setEnabled(Check());
}

void CloneFromGitHubDialog::on_radioButton_ssh_clicked()
{
	updateUI();
}

void CloneFromGitHubDialog::on_radioButton_http_clicked()
{
	updateUI();
}
