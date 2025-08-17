#include "AddRepositoryDialog.h"
#include "ui_AddRepositoryDialog.h"
#include "ApplicationGlobal.h"
#include "Git.h"
#include "MainWindow.h"
#include "ManageWorkingFolderDialog.h"
#include "SearchFromGitHubDialog.h"
#include "common/misc.h"
#include <QFileDialog>
#include <QFocusEvent>
#include <QMessageBox>
#include "common/joinpath.h"

enum {
	Manage = -100,
};

AddRepositoryDialog::AddRepositoryDialog(MainWindow *parent, QString const &local_dir)
	: QDialog(parent)
	, ui(new Ui::AddRepositoryDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	updateWorkingDirComboBoxFolders();

	ui->lineEdit_local_path->setText(local_dir);
	ui->groupBox_remote->setChecked(false);
	resetRemoteRepository();

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

int AddRepositoryDialog::execClone(QString const &remote_url)
{
	mode_ = Clone;
	mode_selectable_ = false;
	ui->lineEdit_remote_repository_url->setText(remote_url);
	ui->stackedWidget->setCurrentWidget(ui->page_first);
	accept();
	return exec();
}

void AddRepositoryDialog::updateWorkingDirComboBoxFolders()
{
	ui->comboBox_local_working_folder->clear();
	bool b = ui->comboBox_local_working_folder->blockSignals(true);

	for (auto const &s : global->appsettings.favorite_working_dirs) {
		ui->comboBox_local_working_folder->addItem(s);
	}

	ui->comboBox_local_working_folder->setCurrentText(global->appsettings.default_working_dir);

	ui->comboBox_local_working_folder->blockSignals(b);

}

QString AddRepositoryDialog::remoteName() const
{
	return ui->lineEdit_remote_name->text();
}

QString AddRepositoryDialog::remoteURL() const
{
	return ui->lineEdit_remote_repository_url->text();
}

void AddRepositoryDialog::setRepositoryNameFromLocalPath()
{
	QString path = ui->lineEdit_local_path->text();
	int i = path.lastIndexOf('/');
	int j = path.lastIndexOf('\\');
	i = std::max(i, j);
	repository_name_ = path.mid(i + 1);
}

void AddRepositoryDialog::setRepositoryNameFromRemoteURL()
{
	QString url = remoteURL();
	auto i = url.lastIndexOf('/');
	auto j = url.lastIndexOf('\\');
	i = std::max(i, j);

	if (i > 0) {
		i++;
	}

	j = url.size();
	if (i + 4 < j && url.endsWith(".git")) {
		j -= 4;
	}

	if (i >= 0 && i < j) {
		repository_name_ = url.mid(i, j - i);
	}
}

QString AddRepositoryDialog::repositoryName() const
{
	return repository_name_;
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

QString AddRepositoryDialog::workingDir() const
{
	return ui->comboBox_local_working_folder->currentText();
}

void AddRepositoryDialog::resetRemoteRepository()
{
	ui->lineEdit_remote_name->setText("origin");
	ui->lineEdit_remote_repository_url->clear();
}

void AddRepositoryDialog::browseLocalPath()
{
	QString dir = workingDir();//ui->lineEdit_local_path->text();
	if (dir.isEmpty()) {
		dir = workingDir();
	}
	dir = QFileDialog::getExistingDirectory(this, tr("Local Path"), dir);
	if (!dir.isEmpty()) {
		dir = misc::normalizePathSeparator(dir);
		ui->lineEdit_local_path->setText(dir);
		QFileInfo info(dir);
		if (info.isDir()) {
			auto i = dir.lastIndexOf('/');
			if (i > 0) {
				dir = dir.mid(0, i);
				bool b = ui->comboBox_local_working_folder->blockSignals(true);
				ui->comboBox_local_working_folder->setItemText(0, dir);
				ui->comboBox_local_working_folder->blockSignals(b);
			}
		}
	}
}

void AddRepositoryDialog::on_pushButton_browse_local_path_clicked()
{
	ui->groupBox_local->setEnabled(true);
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

void AddRepositoryDialog::validate()
{
	QString path = localPath(false);
	QString text;
	if (mainwindow()->isValidWorkingCopy(path)) {
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
}

GitCloneData AddRepositoryDialog::makeCloneData() const
{
	QString url = remoteURL();
	QString dir = localPath(false);
	GitCloneData clonedata = Git::preclone(url, dir);
	return clonedata;
}

RepositoryInfo AddRepositoryDialog::repositoryInfo() const
{
	RepositoryInfo info;
	info.local_dir = localPath(true);
	info.name = repositoryName();
	info.ssh_key = overridedSshKey();
	return info;
}

void AddRepositoryDialog::updateUI()
{
	auto *currentwidget = ui->stackedWidget->currentWidget();
	
	bool prev_enabled = currentwidget != ui->page_first;
	bool ok_button = false;

	switch (mode()) {
	case Clone:
		if (!mode_selectable_ && currentwidget == ui->page_remote) {
			prev_enabled = false;
		}
		ok_button = (currentwidget == ui->page_local);
		break;
	case AddExisting:
		if (!mode_selectable_ && currentwidget == ui->page_local) {
			prev_enabled = false;
		}
		ok_button = (currentwidget == ui->page_local);
		break;
	case Initialize:
		if (!mode_selectable_ && currentwidget == ui->page_local) {
			prev_enabled = false;
		}
		ok_button = (currentwidget == ui->page_remote);
		break;
	}
	if (ok_button) {
		ui->pushButton_ok->setText(tr("OK"));
	} else {
		ui->pushButton_ok->setText(tr("Next"));
	}

	ui->pushButton_prev->setEnabled(prev_enabled);

	ui->comboBox_search->setVisible(currentwidget == ui->page_remote);
}

void AddRepositoryDialog::accept()
{
	auto *currpage = ui->stackedWidget->currentWidget();
	if (currpage == ui->page_first) {
		switch (mode()) {
		case Clone:
			ui->stackedWidget->setCurrentWidget(ui->page_remote);
			ui->groupBox_remote->setCheckable(false);
			ui->groupBox_remote->setEnabled(true);
			ui->groupBox_remote->setChecked(true);
			ui->comboBox_search->setVisible(true);
			ui->lineEdit_remote_repository_url->setFocus();
			ui->groupBox_local->setEnabled(true);
			break;
		case AddExisting:
			ui->groupBox_local->setEnabled(false);
			ui->stackedWidget->setCurrentWidget(ui->page_local);
			ui->groupBox_remote->setEnabled(false);
			ui->groupBox_remote->setChecked(false);
			browseLocalPath();
			break;
		case Initialize:
			ui->groupBox_local->setEnabled(true);
			ui->stackedWidget->setCurrentWidget(ui->page_local);
			ui->groupBox_remote->setEnabled(true);
			ui->groupBox_remote->setCheckable(true);
			ui->groupBox_remote->setChecked(false);
			resetRemoteRepository();
			browseLocalPath();
			break;
		}
		updateUI();
		return;
	} else if (mode() == Clone) {
		if (currpage == ui->page_remote) {
			setRepositoryNameFromRemoteURL();
			updateLocalPath();
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
			setRepositoryNameFromLocalPath();
			ui->stackedWidget->setCurrentWidget(ui->page_remote);
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
		if (mode_selectable_) {
			ui->stackedWidget->setCurrentWidget(ui->page_first);
		}
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
	QString url = remoteURL();
	QString sshkey = overridedSshKey();
	mainwindow()->testRemoteRepositoryValidity(url, sshkey);
	validate();
}

void AddRepositoryDialog::on_radioButton_clone_clicked()
{
	mode_ = Clone;
	updateUI();
}

void AddRepositoryDialog::on_radioButton_add_existing_clicked()
{
	mode_ = AddExisting;
	updateUI();
}

void AddRepositoryDialog::on_radioButton_initialize_clicked()
{
	mode_ = Initialize;
	updateUI();
}

void AddRepositoryDialog::setRemoteURL(QString const &url)
{
	ui->lineEdit_remote_repository_url->setText(url);
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

void AddRepositoryDialog::updateLocalPath()
{
	QString path = workingDir() / repositoryName();
	path = misc::normalizePathSeparator(path);

	bool b = ui->lineEdit_local_path->blockSignals(true);
	ui->lineEdit_local_path->setText(path);
	ui->lineEdit_local_path->blockSignals(b);
}

void AddRepositoryDialog::on_comboBox_local_working_folder_currentTextChanged(const QString &)
{
	updateLocalPath();
}

void AddRepositoryDialog::on_groupBox_remote_clicked()
{
	if (ui->groupBox_remote->isChecked()) {
		ui->lineEdit_remote_repository_url->setFocus();
	}
}

void AddRepositoryDialog::on_pushButton_manage_local_fonder_clicked()
{
	ManageWorkingFolderDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		ui->comboBox_local_working_folder->setCurrentIndex(0);
		updateWorkingDirComboBoxFolders();
	}
}

