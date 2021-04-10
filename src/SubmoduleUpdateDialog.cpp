#include "SubmoduleUpdateDialog.h"
#include "ui_SubmoduleUpdateDialog.h"

SubmoduleUpdateDialog::SubmoduleUpdateDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SubmoduleUpdateDialog)
{
	ui->setupUi(this);
}

SubmoduleUpdateDialog::~SubmoduleUpdateDialog()
{
	delete ui;
}

bool SubmoduleUpdateDialog::isInit() const
{
	return ui->checkBox_init->isChecked();
}

bool SubmoduleUpdateDialog::isRecursive() const
{
	return ui->checkBox_recursive->isChecked();
}
