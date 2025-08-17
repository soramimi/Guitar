#include "ApplicationGlobal.h"
#include "SettingLoggingForm.h"
#include "ui_SettingLoggingForm.h"

#include <QFileDialog>

SettingLoggingForm::SettingLoggingForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingLoggingForm)
{
	ui->setupUi(this);
}

SettingLoggingForm::~SettingLoggingForm()
{
	delete ui;
}

void SettingLoggingForm::exchange(bool save)
{
	if (save) {
		settings()->enable_trace_log = ui->checkBox_trace_log->isChecked();
		settings()->enable_remote_log = ui->checkBox_remote_log->isChecked();
		settings()->remote_log_host = ui->lineEdit_remote_log_host->text();
		settings()->remote_log_port = ui->spinBox_remote_log_port->value();
		settings()->use_custom_log_dir = ui->radioButton_dir_custom->isChecked();
		settings()->custom_log_dir = ui->lineEdit_custom_log_dir->text();
	} else {
		ui->checkBox_trace_log->setChecked(settings()->enable_trace_log);
		ui->checkBox_remote_log->setChecked(settings()->enable_remote_log);
		ui->lineEdit_remote_log_host->setText(settings()->remote_log_host);
		ui->spinBox_remote_log_port->setValue(settings()->remote_log_port);
		ui->radioButton_dir_default->setChecked(!settings()->use_custom_log_dir);
		ui->radioButton_dir_custom->setChecked(settings()->use_custom_log_dir);
		ui->lineEdit_custom_log_dir->setText(settings()->custom_log_dir);
		updateUI();
	}
}

void SettingLoggingForm::updateUI()
{
	ui->frame_custom_dir->setEnabled(ui->radioButton_dir_custom->isChecked());
}

void SettingLoggingForm::on_radioButton_dir_default_clicked()
{
	updateUI();
}

void SettingLoggingForm::on_radioButton_dir_custom_clicked()
{
	updateUI();
}


void SettingLoggingForm::on_pushButton_browse_output_dir_clicked()
{
	QString dir = ui->lineEdit_custom_log_dir->text();
	if (dir.isEmpty()) {
		dir = global->log_dir;
	}
	QString title = tr("Select Log Output Directory");
#ifdef Q_OS_WIN
	dir = QFileDialog::getExistingDirectory(this, title, dir);
#else
	dir = QFileDialog::getExistingDirectory(this, title, dir, QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
#endif
	if (!dir.isEmpty()) {
		ui->lineEdit_custom_log_dir->setText(dir);
	}
}

