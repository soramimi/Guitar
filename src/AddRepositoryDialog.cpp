#include "AddRepositoryDialog.h"
#include "MainWindow.h"
#include "common/misc.h"
#include "SearchFromGitHubDialog.h"
#include "ui_AddRepositoryDialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include "Git.h"

AddRepositoryDialog::AddRepositoryDialog(MainWindow *parent, QString const &dir) :
	QDialog(parent),
	ui(new Ui::AddRepositoryDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	already_exists_ = tr("A valid git repository exists.");

	ui->lineEdit_local_path->setText(dir);
	ui->groupBox_remote->setChecked(false);
	ui->lineEdit_remote_name->setText("origin");

	ui->radioButton_clone->click();

	ui->comboBox_search->addItem(tr("Search"));
	ui->comboBox_search->addItem(tr("GitHub"));

	validate(false);
}

AddRepositoryDialog::~AddRepositoryDialog()
{
	delete ui;
}

MainWindow *AddRepositoryDialog::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
}

MainWindow const *AddRepositoryDialog::mainwindow() const
{
	return const_cast<AddRepositoryDialog *>(this)->mainwindow();
}

AddRepositoryDialog::Mode AddRepositoryDialog::mode() const
{
	return mode_;
}

void AddRepositoryDialog::accept()
{
	QString path = ui->lineEdit_local_path->text();
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

QString AddRepositoryDialog::defaultWorkingDir() const
{
	return mainwindow()->defaultWorkingDir();
}

QString AddRepositoryDialog::browseLocalPath()
{
	QString dir = ui->lineEdit_local_path->text();
	if (dir.isEmpty()) {
		dir = defaultWorkingDir();
	}
	dir = QFileDialog::getExistingDirectory(this, tr("Local Path"), dir);
	if (!dir.isEmpty()) {
		dir = misc::normalizePathSeparator(dir);
		ui->lineEdit_local_path->setText(dir);
	}
	return dir;
}

void AddRepositoryDialog::on_pushButton_browse_local_path_clicked()
{
	browseLocalPath();
}

QString AddRepositoryDialog::localPath() const
{
	return ui->lineEdit_local_path->text();
}

QString AddRepositoryDialog::name() const
{
	return ui->lineEdit_bookmark_name->text();
}

QString AddRepositoryDialog::remoteName() const
{
	return ui->groupBox_remote->isChecked() ? ui->lineEdit_remote_name->text() : QString();
}

QString AddRepositoryDialog::remoteURL() const
{
	return ui->groupBox_remote->isChecked() ? ui->lineEdit_remote_url->text() : QString();
}

void AddRepositoryDialog::validate(bool change_name)
{
	QString path = localPath();
	{
		QString text;
		if (Git::isValidWorkingCopy(path)) {
			text = already_exists_;
		}
		ui->label_warning->setText(text);
	}

	if (change_name) {
		if (path == defaultWorkingDir()) {
			ui->lineEdit_bookmark_name->setText({});
		} else {
			auto i = path.lastIndexOf('/');
			auto j = path.lastIndexOf('\\');
			if (i < j) i = j;

			if (i >= 0) {
				QString name = path.mid(i + 1);
				ui->lineEdit_bookmark_name->setText(name);
			}
		}
	}
}

QString AddRepositoryDialog::overridedSshKey()
{
	return ui->widget_ssh_override->sshKey();
}

void AddRepositoryDialog::on_lineEdit_local_path_textChanged(QString const &)
{
	validate(true);
}

void AddRepositoryDialog::on_lineEdit_bookmark_name_textChanged(QString const &)
{
	validate(false);
}

void AddRepositoryDialog::on_groupBox_remote_toggled(bool)
{
	validate(false);
}

void AddRepositoryDialog::on_pushButton_test_repo_clicked()
{
	QString url = ui->lineEdit_remote_url->text();
	QString sshkey = overridedSshKey();
	mainwindow()->testRemoteRepositoryValidity(url, sshkey);
	validate(false);
}

void AddRepositoryDialog::updateUI()
{
	ui->comboBox_search->setEnabled(mode_ == Clone);

	if (mode_ == Clone) {
		ui->groupBox_remote->setChecked(true);
	}

	if (mode_ == Initialize) {
		ui->comboBox_search->setEnabled(false);
	}
}

void AddRepositoryDialog::on_radioButton_clone_clicked()
{
	mode_ = Clone;
	updateUI();

	ui->lineEdit_local_path->setText(defaultWorkingDir());

	ui->lineEdit_remote_url->setFocus();
}

void AddRepositoryDialog::on_radioButton_init_clicked()
{
	mode_ = Initialize;
	updateUI();

	ui->lineEdit_local_path->setFocus();
}

void AddRepositoryDialog::on_radioButton_add_existing_clicked()
{
	QString dir = browseLocalPath();
	if (global->mainwindow->addExistingLocalRepository(dir, true)) {
		done(QDialog::Accepted);
		return;
	}

	ui->radioButton_clone->click();
}

void AddRepositoryDialog::setRemoteURL(QString const &url)
{
	ui->lineEdit_remote_url->setText(url);
}

void AddRepositoryDialog::on_comboBox_search_currentIndexChanged(int index)
{
	if (index == GitHub) {
		SearchFromGitHubDialog dlg(this, mainwindow());
		if (dlg.exec() == QDialog::Accepted) {
			setRemoteURL(dlg.url());
		}
	}
	ui->comboBox_search->setCurrentIndex(0);
}

void AddRepositoryDialog::on_lineEdit_remote_url_textChanged(const QString &text)
{
	QString reponame;
	auto i = text.lastIndexOf('/');
	auto j = text.lastIndexOf('\\');
	if (i < j) i = j;

	if (i > 0) {
		i++;
	}

	j = text.size();
	if (i + 4 < j && text.endsWith(".git")) {
		j -= 4;
	}

	if (i >= 0 && i < j) {
		reponame = text.mid(i, j - i);
	}
	ui->lineEdit_bookmark_name->setText(reponame);

	QString path = defaultWorkingDir() / reponame;
	path = misc::normalizePathSeparator(path);
	ui->lineEdit_local_path->setText(path);
}


