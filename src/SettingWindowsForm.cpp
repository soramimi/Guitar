#include "SettingWindowsForm.h"
#include "ui_SettingWindowsForm.h"
#include "MySettings.h"
#include "ApplicationGlobal.h"

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
		ApplicationSettings::ConsoleBackend backend = ApplicationSettings::ConsoleBackend::ConPtyDirectly;
		if (ui->radioButton_conpty_with_worker_process->isChecked()) {
			backend = ApplicationSettings::ConsoleBackend::ConPtyWithWorkerProcess;
		} else if (ui->radioButton_winpty->isChecked()) {
			backend = ApplicationSettings::ConsoleBackend::WinPty;
		}
		settings()->console_backend = backend;
	} else {
		ui->radioButton_conpty_directly->setChecked(settings()->console_backend == ApplicationSettings::ConsoleBackend::ConPtyDirectly);
		ui->radioButton_conpty_with_worker_process->setChecked(settings()->console_backend == ApplicationSettings::ConsoleBackend::ConPtyWithWorkerProcess);
		ui->radioButton_winpty->setChecked(settings()->console_backend == ApplicationSettings::ConsoleBackend::WinPty);
	}
}
