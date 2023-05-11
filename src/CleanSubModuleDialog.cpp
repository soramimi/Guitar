#include "CleanSubModuleDialog.h"
#include "ui_CleanSubModuleDialog.h"

CleanSubModuleDialog::CleanSubModuleDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::CleanSubModuleDialog)
{
	ui->setupUi(this);

	ui->checkBox_reset_hard->setChecked(options_.reset_hard);
	ui->checkBox_clean_df->setChecked(options_.clean_df);
}

CleanSubModuleDialog::~CleanSubModuleDialog()
{
	delete ui;
}

CleanSubModuleDialog::Options CleanSubModuleDialog::options() const
{
	return options_;
}

void CleanSubModuleDialog::on_checkBox_reset_hard_stateChanged(int)
{
	options_.reset_hard = ui->checkBox_reset_hard->isChecked();
}

void CleanSubModuleDialog::on_checkBox_clean_df_stateChanged(int)
{
	options_.clean_df = ui->checkBox_clean_df->isChecked();
}

