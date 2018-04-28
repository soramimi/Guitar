#include "CommitPropertyDialog.h"
#include "MainWindow.h"
#include "ui_CommitPropertyDialog.h"

#include "common/misc.h"

struct CommitPropertyDialog::Private {
	MainWindow *mainwindow;
	Git::CommitItem commit;
};

void CommitPropertyDialog::init(MainWindow *mw)
{
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->pushButton_jump->setVisible(false);

	m->mainwindow = mw;

	ui->lineEdit_description->setText(m->commit.message);
	ui->lineEdit_commit_id->setText(m->commit.commit_id);
	ui->lineEdit_date->setText(misc::makeDateTimeString(m->commit.commit_date));
	ui->lineEdit_author->setText(m->commit.author);
	ui->lineEdit_mail->setText(m->commit.email);

	QString text;
	for (QString const &id : m->commit.parent_ids) {
		text += id + '\n';
	}
	ui->plainTextEdit_parent_ids->setPlainText(text);
}

CommitPropertyDialog::CommitPropertyDialog(QWidget *parent, MainWindow *mw, Git::CommitItem const *commit)
	: QDialog(parent)
	, ui(new Ui::CommitPropertyDialog)
	, m(new Private)
{
	ui->setupUi(this);
	m->commit = *commit;
	init(mw);
}

CommitPropertyDialog::CommitPropertyDialog(QWidget *parent, MainWindow *mw, const QString &commit_id)
	: QDialog(parent)
	, ui(new Ui::CommitPropertyDialog)
	, m(new Private)
{
	ui->setupUi(this);

	mw->queryCommit(commit_id, &m->commit);
	init(mw);
}

CommitPropertyDialog::~CommitPropertyDialog()
{
	delete m;
	delete ui;
}

void CommitPropertyDialog::showCheckoutButton(bool f)
{
	ui->pushButton_checkout->setVisible(f);
}

void CommitPropertyDialog::showJumpButton(bool f)
{
	ui->pushButton_jump->setVisible(f);
}

void CommitPropertyDialog::on_pushButton_checkout_clicked()
{
	m->mainwindow->checkout(this, &m->commit);
	done(QDialog::Rejected);
}

void CommitPropertyDialog::on_pushButton_jump_clicked()
{
	m->mainwindow->jumpToCommit(m->commit.commit_id);
	done(QDialog::Accepted);
}

void CommitPropertyDialog::on_pushButton_details_clicked()
{
	m->mainwindow->execCommitViewWindow(&m->commit);
}

void CommitPropertyDialog::on_pushButton_explorer_clicked()
{
	m->mainwindow->execCommitExploreWindow(this, &m->commit);

}
