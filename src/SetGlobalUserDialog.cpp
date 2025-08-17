#include "SetGlobalUserDialog.h"
#include "ui_SetGlobalUserDialog.h"

#include "common/misc.h"

SetGlobalUserDialog::SetGlobalUserDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SetGlobalUserDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	misc::setFixedSize(this);

	ui->lineEdit_name->setFocus();
}

SetGlobalUserDialog::~SetGlobalUserDialog()
{
	delete ui;
}

GitUser SetGlobalUserDialog::user() const
{
	GitUser user;
	user.name = ui->lineEdit_name->text();
	user.email = ui->lineEdit_mail->text();
	return user;
}

