#include "CreateRepositoryDialog.h"
#include "MainWindow.h"
#include "misc.h"
#include "ui_CreateRepositoryDialog.h"

#include <QFileDialog>
#include "Git.h"

CreateRepositoryDialog::CreateRepositoryDialog(MainWindow *parent) :
	QDialog(parent),
	ui(new Ui::CreateRepositoryDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	mainwindow_ = parent;

	ui->groupBox_remote->setChecked(false);

	validate();
}

CreateRepositoryDialog::~CreateRepositoryDialog()
{
	delete ui;
}

void CreateRepositoryDialog::on_pushButton_browse_path_clicked()
{
	QString path = QFileDialog::getExistingDirectory(this, tr("Destination Path"), mainwindow_->defaultWorkingDir());
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

QString CreateRepositoryDialog::remoteURL() const
{
	if (remote_url_is_ok_) {
		return ui->lineEdit_remote_url->text();
	}
	return QString();
}

void CreateRepositoryDialog::validate()
{
	bool ok = true;
	QString path = ui->lineEdit_path->text();
	if (Git::isValidWorkingCopy(path)) {
		ui->label_warning->setText(tr("A valid git repository already exists there."));
		ok = false;
	}

	int i = path.lastIndexOf('/');
	int j = path.lastIndexOf('\\');
	if (i < j) i = j;

	if (i >= 0) {
		QString name = path.mid(i + 1);
		ui->lineEdit_name->setText(name);
		if (ui->groupBox_bookmark->isChecked() && name.isEmpty()) {
			ok = false;
		}
	}

	if (!QFileInfo(path).isDir()) {
		ok = false;
	}

	if (ui->groupBox_remote->isChecked() && !remote_url_is_ok_) {
		ok = false;
	}

	ui->pushButton_ok->setEnabled(ok);
}

void CreateRepositoryDialog::on_lineEdit_path_textChanged(const QString &)
{
	validate();
}

void CreateRepositoryDialog::on_lineEdit_name_textChanged(const QString &)
{
	validate();
}

void CreateRepositoryDialog::on_groupBox_remote_toggled(bool)
{
	remote_url_is_ok_ = false;
	validate();
}

void CreateRepositoryDialog::on_pushButton_test_repo_clicked()
{
	QString url = ui->lineEdit_remote_url->text();
	remote_url_is_ok_ = mainwindow_->testRemoteRepositoryValidity(url);
	validate();
}
