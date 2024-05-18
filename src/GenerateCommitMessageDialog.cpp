#include "GenerateCommitMessageDialog.h"
#include "OverrideWaitCursor.h"
#include "ui_GenerateCommitMessageDialog.h"
#include "CommitMessageGenerator.h"
#include "ApplicationGlobal.h"
#include "MainWindow.h"

GenerateCommitMessageDialog::GenerateCommitMessageDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::GenerateCommitMessageDialog)
{
	ui->setupUi(this);
}

GenerateCommitMessageDialog::~GenerateCommitMessageDialog()
{
	delete ui;
}

QString GenerateCommitMessageDialog::text() const
{
	return ui->listWidget->currentItem()->text();
}

void GenerateCommitMessageDialog::generate()
{
	QStringList list;
	{
		OverrideWaitCursor;
		CommitMessageGenerator gen;
		list = gen.generate(global->mainwindow->git());
	}
	if (!list.isEmpty()) {
		ui->listWidget->addItems(list);
		ui->listWidget->setCurrentRow(0);
	}
}

void GenerateCommitMessageDialog::on_pushButton_regenerate_clicked()
{
	generate();
}
