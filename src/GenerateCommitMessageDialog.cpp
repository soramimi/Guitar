#include "GenerateCommitMessageDialog.h"
#include "ui_ChoiceCommitMessageDialog.h"
#include "CommitMessageGenerator.h"
#include "ApplicationGlobal.h"
#include "MainWindow.h"

GenerateCommitMessageDialog::GenerateCommitMessageDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::GenerateCommitMessageWithAiDialog)
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
	qApp->setOverrideCursor(Qt::WaitCursor);

	CommitMessageGenerator gen;
	auto list = gen.generate(global->mainwindow->git());

	qApp->restoreOverrideCursor();

	if (!list.isEmpty()) {
		ui->listWidget->addItems(list);
		ui->listWidget->setCurrentRow(0);
	}
}


