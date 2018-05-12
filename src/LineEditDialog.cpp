#include "LineEditDialog.h"
#include "ui_LineEditDialog.h"

LineEditDialog::LineEditDialog(QWidget *parent, const QString &title, const QString &prompt, bool password) :
	QDialog(parent),
	ui(new Ui::LineEditDialog)
{
	ui->setupUi(this);
	setWindowTitle(title);
	ui->label_prompt->setText(prompt);
	if (password) {
		ui->lineEdit->setEchoMode(QLineEdit::Password);
	}
}

LineEditDialog::~LineEditDialog()
{
	delete ui;
}

QString LineEditDialog::text() const
{
	return ui->lineEdit->text();
}
