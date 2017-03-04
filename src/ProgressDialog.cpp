#include "ProgressDialog.h"
#include "ui_ProgressDialog.h"

ProgressDialog::ProgressDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::ProgressDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags &= ~Qt::WindowCloseButtonHint;
	setWindowFlags(flags);}

ProgressDialog::~ProgressDialog()
{
	delete ui;
}

void ProgressDialog::setLabelText(const QString &text)
{
	ui->label->setText(text);
}

void ProgressDialog::on_pushButton_cancel_clicked()
{
	canceled_by_user = true;
}

bool ProgressDialog::isCanceledByUser() const
{
	return canceled_by_user;
}
