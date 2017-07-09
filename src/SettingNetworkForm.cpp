#include "SettingNetworkForm.h"
#include "ui_SettingNetworkForm.h"
#include "SettingsDialog.h"

SettingNetworkForm::SettingNetworkForm(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingNetworkForm)
{
	ui->setupUi(this);
}

SettingNetworkForm::~SettingNetworkForm()
{
	delete ui;
}

void SettingNetworkForm::reflect()
{
	ApplicationSettings const *s = settings();
	if (s->proxy_type == "auto") {
		ui->radioButton_auto_detect->click();
	} else if (s->proxy_type == "manual") {
		ui->radioButton_manual->click();
	} else {
		ui->radioButton_no_proxy->click();
	}
	ui->lineEdit_proxy_server->setText(s->proxy_server);
}

void SettingNetworkForm::on_radioButton_no_proxy_clicked()
{
	settings()->proxy_type = "none";
}

void SettingNetworkForm::on_radioButton_auto_detect_clicked()
{
	settings()->proxy_type = "auto";
}

void SettingNetworkForm::on_radioButton_manual_clicked()
{
	settings()->proxy_type = "manual";
}

void SettingNetworkForm::on_lineEdit_proxy_server_textChanged(const QString &text)
{
	settings()->proxy_server = text;
}
