#include "SettingWindowsForm.h"
#include "ui_SettingWindowsForm.h"
#include "MySettings.h"
// #include "ApplicationGlobal.h"

SettingWindowsForm::SettingWindowsForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingWindowsForm)
{
	ui->setupUi(this);
}

SettingWindowsForm::~SettingWindowsForm()
{
	delete ui;
}

void SettingWindowsForm::exchange(bool save)
{
	if (save) {
		ApplicationSettings::ConsoleBackend backend = ApplicationSettings::ConsoleBackend::ConPty;
		if (ui->radioButton_conpty_with_worker->isChecked()) {
			backend = ApplicationSettings::ConsoleBackend::ConPtyWithWorker;
		} else if (ui->radioButton_winpty->isChecked()) {
			backend = ApplicationSettings::ConsoleBackend::WinPty;
		}
		settings()->console_backend = backend;
	} else {
		ui->radioButton_conpty->setChecked(settings()->console_backend == ApplicationSettings::ConsoleBackend::ConPty);
		ui->radioButton_conpty_with_worker->setChecked(settings()->console_backend == ApplicationSettings::ConsoleBackend::ConPtyWithWorker);
		ui->radioButton_winpty->setChecked(settings()->console_backend == ApplicationSettings::ConsoleBackend::WinPty);
	}
}
