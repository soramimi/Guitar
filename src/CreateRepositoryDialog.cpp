#include "CreateRepositoryDialog.h"
#include "BasicMainWindow.h"
#include "common/misc.h"
#include "ui_CreateRepositoryDialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include "Git.h"

CreateRepositoryDialog::CreateRepositoryDialog(BasicMainWindow *parent, QString const &dir) :
	QDialog(parent),
	ui(new Ui::CreateRepositoryDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	already_exists_ = tr("A valid git repository already exists there.");

	ui->lineEdit_path->setText(dir);
	ui->groupBox_remote->setChecked(false);
	ui->lineEdit_remote_name->setText("origin");

	validate(false);
}

CreateRepositoryDialog::~CreateRepositoryDialog()
{
	delete ui;
}

BasicMainWindow *CreateRepositoryDialog::mainwindow()
{
	return qobject_cast<BasicMainWindow *>(parent());
}

void CreateRepositoryDialog::accept()
{
	QString path = ui->lineEdit_path->text();
	if (!QFileInfo(path).isDir()) {
		QMessageBox::warning(this, tr("Create Repository"), tr("The specified path is not a directory."));
		return;
	}
	if (Git::isValidWorkingCopy(path)) {
		QMessageBox::warning(this, tr("Create Repository"), already_exists_);
		return;
	}
	if (!QFileInfo(path).isDir()) {
		QMessageBox::warning(this, tr("Create Repository"), tr("The specified path is not a directory."));
		return;
	}
	if (remoteName().indexOf('\"') >= 0 || remoteURL().indexOf('\"') >= 0) { // 手抜き
		QMessageBox::warning(this, tr("Create Repository"), tr("Remote name is invalid."));
		return;
	}
	done(QDialog::Accepted);
}

void CreateRepositoryDialog::on_pushButton_browse_path_clicked()
{
	QString path = QFileDialog::getExistingDirectory(this, tr("Destination Path"), mainwindow()->defaultWorkingDir());
	if (!path.isEmpty()) {
		path = misc::normalizePathSeparator(path);
		ui->lineEdit_path->setText(path);
	}
}

QString CreateRepositoryDialog::path() const
{
	return ui->lineEdit_path->text();
}

QString CreateRepositoryDialog::name() const
{
	return ui->lineEdit_name->text();
}

QString CreateRepositoryDialog::remoteName() const
{
	return ui->groupBox_remote->isChecked() ? ui->lineEdit_remote_name->text() : QString();
}

QString CreateRepositoryDialog::remoteURL() const
{
	return ui->groupBox_remote->isChecked() ? ui->lineEdit_remote_url->text() : QString();
}

void CreateRepositoryDialog::validate(bool change_name)
{
	QString path = this->path();
	{
		QString text;
		if (Git::isValidWorkingCopy(path)) {
			text = already_exists_;
		}
		ui->label_warning->setText(text);
	}

	if (change_name) {
		int i = path.lastIndexOf('/');
		int j = path.lastIndexOf('\\');
		if (i < j) i = j;

		if (i >= 0) {
			QString name = path.mid(i + 1);
			ui->lineEdit_name->setText(name);
		}
	}
}

void CreateRepositoryDialog::on_lineEdit_path_textChanged(QString const &)
{
	validate(true);
}

void CreateRepositoryDialog::on_lineEdit_name_textChanged(QString const &)
{
	validate(false);
}

void CreateRepositoryDialog::on_groupBox_remote_toggled(bool)
{
	validate(false);
}

void CreateRepositoryDialog::on_pushButton_test_repo_clicked()
{
	QString url = ui->lineEdit_remote_url->text();
	mainwindow()->testRemoteRepositoryValidity(url, {});
	validate(false);
}
