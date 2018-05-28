#include "TagEditDialog.h"
#include "ui_TagEditDialog.h"

TagEditDialog::TagEditDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::TagEditDialog)
{
	ui->setupUi(this);
}

TagEditDialog::~TagEditDialog()
{
	delete ui;
}
