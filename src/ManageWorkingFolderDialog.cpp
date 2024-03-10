#include "ManageWorkingFolderDialog.h"
#include "ui_ManageWorkingFolderDialog.h"

ManageWorkingFolderDialog::ManageWorkingFolderDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::ManageWorkingFolderDialog)
{
	ui->setupUi(this);
	settings_ = global->appsettings;
	SettingWorkingFolderForm::loadFavoliteDirs(&settings_.favorite_working_dirs);
	ui->widget->reset(nullptr, &settings_);
	ui->widget->exchange(false);
}

ManageWorkingFolderDialog::~ManageWorkingFolderDialog()
{
	delete ui;
}

void ManageWorkingFolderDialog::accept()
{
	ui->widget->exchange(true);
	SettingWorkingFolderForm::saveFavoliteDirs(settings_.favorite_working_dirs);
	global->appsettings.favorite_working_dirs = settings_.favorite_working_dirs;
	QDialog::accept();
}
