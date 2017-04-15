#include "AskPassDialog.h"
#include "ui_AskPassDialog.h"

AskPassDialog::AskPassDialog(QString const &caption)
	: QDialog(nullptr)
	, ui(new Ui::AskPassDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags |= Qt::WindowStaysOnTopHint;
	setWindowFlags(flags);

	ui->label->setText(caption);

	if (caption.startsWith("password", Qt::CaseInsensitive)) {
		ui->lineEdit->setEchoMode(QLineEdit::Password);
	}
}

AskPassDialog::~AskPassDialog()
{
	delete ui;
}

QString AskPassDialog::text() const
{
	return ui->lineEdit->text();
}
