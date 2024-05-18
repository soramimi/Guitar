#include "ChoiceCommitMessageDialog.h"
#include "ui_ChoiceCommitMessageDialog.h"

ChoiceCommitMessageDialog::ChoiceCommitMessageDialog(const QStringList &list, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::GenerateCommitMessageWithAiDialog)
{
	ui->setupUi(this);
	ui->listWidget->addItems(list);
	ui->listWidget->setCurrentRow(0);
}

ChoiceCommitMessageDialog::~ChoiceCommitMessageDialog()
{
	delete ui;
}

QString ChoiceCommitMessageDialog::text() const
{
	return ui->listWidget->currentItem()->text();
}


