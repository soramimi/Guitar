#include "EditProfileDialog.h"
#include "ui_EditProfileDialog.h"

EditProfileDialog::EditProfileDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::EditProfileDialog)
{
	ui->setupUi(this);
}

EditProfileDialog::~EditProfileDialog()
{
	delete ui;
}
