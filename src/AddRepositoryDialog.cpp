#include "AddRepositoryDialog.h"
#include "ui_AddRepositoryDialog.h"
#include "Git.h"
#include "MainWindow.h"
#include "SearchFromGitHubDialog.h"
#include "common/misc.h"
#include <QFileDialog>
#include <QFocusEvent>
#include <QMessageBox>

AddRepositoryDialog::AddRepositoryDialog(MainWindow *parent, QString const &dir)
	: QDialog(parent)
	, ui(new Ui::AddRepositoryDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	already_exists_ = tr("A valid git repository exists.");

	ui->lineEdit_local_path->setText(dir);
	ui->groupBox_remote->setChecked(false);
	ui->lineEdit_remote_name->setText("origin");

	ui->comboBox_search->addItem(tr("Search"));
	ui->comboBox_search->addItem(tr("GitHub"));

	validate();

	ui->stackedWidget->setCurrentWidget(ui->page_first);
	ui->radioButton_clone->setFocus();
	ui->radioButton_clone->click();
	updateUI();
}

AddRepositoryDialog::~AddRepositoryDialog()
{
	delete ui;
}

QString AddRepositoryDialog::repositoryName() const
{
	return reponame_;
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

QString AddRepositoryDialog::defaultWorkingDir() const
{
	return mainwindow()->defaultWorkingDir();
}

void AddRepositoryDialog::browseLocalPath()
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
}

void AddRepositoryDialog::on_pushButton_browse_local_path_clicked()
{
	browseLocalPath();
}

QString AddRepositoryDialog::localPath(bool cook) const
{
	QString s = ui->lineEdit_local_path->text();
	if (cook) {
		s = s.replace('\\', '/');
	}
	return s;

}

QString AddRepositoryDialog::remoteName() const
{
	return ui->lineEdit_remote_name->text();
}

QString AddRepositoryDialog::remoteURL() const
{
	return ui->lineEdit_remote_url->text();
}

void AddRepositoryDialog::validate()
{
	QString path = localPath(false);
	{
		QString text;
		if (Git::isValidWorkingCopy(path)) {
			switch (mode()) {
			case AddRepositoryDialog::Clone:
				text = tr("A valid git repository already exists.");
				break;
			case AddRepositoryDialog::Initialize:
				break;
			case AddRepositoryDialog::AddExisting:
				text = tr("A valid git repository.");
				break;
			}

		}
		ui->label_warning->setText(text);
	}
}

void AddRepositoryDialog::updateUI()
{
	ui->pushButton_prev->setEnabled(ui->stackedWidget->currentWidget() != ui->page_first);

	bool okbutton = false;
	switch (mode()) {
	case Clone:
		okbutton = (ui->stackedWidget->currentWidget() == ui->page_local);
		break;
	case Initialize:
		okbutton = (ui->stackedWidget->currentWidget() == ui->page_remote);
		break;
	case AddExisting:
		okbutton = (ui->stackedWidget->currentWidget() == ui->page_local);
		break;
	}
	if (okbutton) {
		ui->pushButton_ok->setText(tr("OK"));
	} else {
		ui->pushButton_ok->setText(tr("Next"));
	}
}

void AddRepositoryDialog::accept()
{
	auto *currpage = ui->stackedWidget->currentWidget();
	if (currpage == ui->page_first) {
		switch (mode()) {
		case Clone:
			ui->lineEdit_local_path->setText(defaultWorkingDir());
			ui->stackedWidget->setCurrentWidget(ui->page_remote);
			ui->groupBox_remote->setCheckable(false);
			ui->comboBox_search->setVisible(true);
			ui->lineEdit_remote_url->setFocus();
			break;
		case AddExisting:
			ui->stackedWidget->setCurrentWidget(ui->page_local);
			browseLocalPath();
			break;
		case Initialize:
			ui->stackedWidget->setCurrentWidget(ui->page_local);
			browseLocalPath();
			break;
		}
		updateUI();
		return;
	} else if (mode() == Clone) {
		if (currpage == ui->page_remote) {
			ui->stackedWidget->setCurrentWidget(ui->page_local);
		} else if (currpage == ui->page_local) {
			done(QDialog::Accepted);
			return;
		}
		updateUI();
		return;
	} else if (mode() == AddExisting) {
		if (currpage == ui->page_local) {
			done(QDialog::Accepted);
		}
		updateUI();
		return;
	} else if (mode() == Initialize) {
		if (currpage == ui->page_local) {
			ui->stackedWidget->setCurrentWidget(ui->page_remote);
			ui->groupBox_remote->setCheckable(true);
			ui->groupBox_remote->setChecked(false);
			ui->comboBox_search->setVisible(false);
		} else if (currpage == ui->page_remote) {
			done(QDialog::Accepted);
		}
		updateUI();
		return;
	}
}

void AddRepositoryDialog::on_pushButton_prev_clicked()
{
	auto *currpage = ui->stackedWidget->currentWidget();
	if (mode() == Clone && currpage == ui->page_local) {
		ui->stackedWidget->setCurrentWidget(ui->page_remote);
	} else if (mode() == Initialize && currpage == ui->page_remote) {
		ui->stackedWidget->setCurrentWidget(ui->page_local);
	} else if (currpage != ui->page_first) {
		ui->stackedWidget->setCurrentWidget(ui->page_first);
	}

	updateUI();
}

QString AddRepositoryDialog::overridedSshKey() const
{
	return ui->widget_ssh_override->sshKey();
}

void AddRepositoryDialog::on_lineEdit_local_path_textChanged(QString const &)
{
	validate();
}

void AddRepositoryDialog::on_groupBox_remote_toggled(bool)
{
	validate();
}

void AddRepositoryDialog::on_pushButton_test_repo_clicked()
{
	QString url = ui->lineEdit_remote_url->text();
	QString sshkey = overridedSshKey();
	mainwindow()->testRemoteRepositoryValidity(url, sshkey);
	validate();
}

void AddRepositoryDialog::on_radioButton_clone_clicked()
{
	mode_ = Clone;
}

void AddRepositoryDialog::on_radioButton_initialize_clicked()
{
	mode_ = Initialize;
}

void AddRepositoryDialog::on_radioButton_add_existing_clicked()
{
	mode_ = AddExisting;
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
		reponame_ = text.mid(i, j - i);
	}

	if (mode() == Clone) {
		QString path = defaultWorkingDir() / reponame_;
		path = misc::normalizePathSeparator(path);
		ui->lineEdit_local_path->setText(path);
	}
}

Git::CloneData AddRepositoryDialog::makeCloneData() const
{
	QString url = remoteURL();
	QString dir = localPath(false);
	Git::CloneData clonedata = Git::preclone(url, dir);
	return clonedata;
}

RepositoryData AddRepositoryDialog::makeRepositoryData() const
{
	RepositoryData reposdata;
	reposdata.local_dir = localPath(true);
	reposdata.name = repositoryName();
	reposdata.ssh_key = overridedSshKey();
	return reposdata;
}

void AddRepositoryDialog::on_groupBox_remote_clicked()
{
	if (ui->groupBox_remote->isChecked()) {
		ui->lineEdit_remote_url->setFocus();
	}
}

