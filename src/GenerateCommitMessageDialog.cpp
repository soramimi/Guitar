#include "GenerateCommitMessageDialog.h"
#include "ui_GenerateCommitMessageDialog.h"
#include "ApplicationGlobal.h"
#include "CommitMessageGenerator.h"
#include "MainWindow.h"
#include "GenerateCommitMessageThread.h"
#include <QMessageBox>

struct GenerateCommitMessageDialog::Private {
	QString diff;
	GenerateCommitMessageThread generator;
};

GenerateCommitMessageDialog::GenerateCommitMessageDialog(QWidget *parent, QString const &model_name)
	: QDialog(parent)
	, ui(new Ui::GenerateCommitMessageDialog)
	, m(new Private)
{
	ui->setupUi(this);
	ui->label->setText(model_name);
	
	m->generator.start();
	
	connect(&m->generator, &GenerateCommitMessageThread::ready, this, &GenerateCommitMessageDialog::onReady);
	
}

GenerateCommitMessageDialog::~GenerateCommitMessageDialog()
{
	m->generator.stop();
	delete ui;
	delete m;
}

void GenerateCommitMessageDialog::generate(QString const &diff)
{
	m->diff = diff;
	
	QApplication::setOverrideCursor(Qt::WaitCursor);

	ui->listWidget->clear();
	ui->pushButton_regenerate->setEnabled(false);
	
	m->generator.request(CommitMessageGenerator::CommitMessage, diff);
}

QString GenerateCommitMessageDialog::diffText() const
{
	return m->diff;
}

QString GenerateCommitMessageDialog::message() const
{
	return ui->listWidget->currentItem()->text();
}

void GenerateCommitMessageDialog::on_pushButton_regenerate_clicked()
{
	QString diff = CommitMessageGenerator::diff_head();
	generate(diff);
}

void GenerateCommitMessageDialog::onReady(const GeneratedCommitMessage &result)
{
	QApplication::restoreOverrideCursor();

	if (result) {
		ui->listWidget->addItems(result.messages);
		ui->listWidget->setCurrentRow(0);
	} else {
		QString text = result.error_status + "\n\n" + result.error_message;
		QMessageBox::warning(this, tr("Failed to generate commit message."), text);
	}
	
	ui->pushButton_regenerate->setEnabled(true);	
}



