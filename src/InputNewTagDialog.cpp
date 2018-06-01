#include "InputNewTagDialog.h"
#include "ui_InputNewTagDialog.h"
#include "common/misc.h"

InputNewTagDialog::InputNewTagDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::InputNewTagDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	misc::setFixedSize(this);
}

InputNewTagDialog::~InputNewTagDialog()
{
	delete ui;
}

QString InputNewTagDialog::text() const
{
	return ui->lineEdit->text();
}


