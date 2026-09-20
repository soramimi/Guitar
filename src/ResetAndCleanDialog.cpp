#include "ResetAndCleanDialog.h"
#include "ui_ResetAndCleanDialog.h"
#include "MainWindow.h"

ResetAndCleanDialog::ResetAndCleanDialog(
	QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::ResetAndCleanDialog)
{
	ui->setupUi(this);
}

ResetAndCleanDialog::~ResetAndCleanDialog()
{
	delete ui;
}

bool ResetAndCleanDialog::perform(MainWindow *mainwindow)
{
	bool ret = false;
	GitRunner g = mainwindow->git();
	if (ui->checkBox_reset_hard->isChecked()) {
		g.reset_hard();
		ret = true;
	}
	if (ui->checkBox_clean_df->isChecked()) {
		g.clean_df();
		ret = true;
	}
	if (ui->checkBox_fetch_prune->isChecked()) {
		mainwindow->fetch(g, true);
		ret = true;
	}
	return ret;
}
