#include "AddRepositoryDialog.h"
#include "MainWindow.h"
#include "common/misc.h"
#include "SearchFromGitHubDialog.h"
#include "ui_AddRepositoryDialog.h"

#include <QFileDialog>
#include <QFocusEvent>
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

	ui->lineEdit_remote_url->installEventFilter(this);

	already_exists_ = tr("A valid git repository exists.");

	ui->lineEdit_local_path->setText(dir);
	ui->groupBox_remote->setChecked(false);
	ui->lineEdit_remote_name->setText("origin");

	ui->comboBox_search->addItem(tr("Search"));
	ui->comboBox_search->addItem(tr("GitHub"));

	validate(false);

	ui->stackedWidget->setCurrentWidget(ui->page_first);
	ui->radioButton_clone->click();
	ui->radioButton_clone->setFocus();
	updateUI();
}

AddRepositoryDialog::~AddRepositoryDialog()
{
	delete ui;
}

bool AddRepositoryDialog::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::FocusOut) {
		if (watched == ui->lineEdit_remote_url) {
			complementRemoteURL(false);
		}
	}
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *e = static_cast<QKeyEvent *>(event);
		if (e->key() == Qt::Key_Space && (e->modifiers() & Qt::ControlModifier)) {
			complementRemoteURL(true);
		}
	}
	return QDialog::eventFilter(watched, event);
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

void AddRepositoryDialog::validate(bool change_name)
{
	QString path = localPath(false);
	{
		QString text;
		if (Git::isValidWorkingCopy(path)) {
			text = already_exists_;
		}
		ui->label_warning->setText(text);
	}

//	if (change_name) {
//		if (path != defaultWorkingDir()) {
//			auto i = path.lastIndexOf('/');
//			auto j = path.lastIndexOf('\\');
//			if (i < j) i = j;
//		}
//	}
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
		okbutton = (ui->stackedWidget->currentWidget() == ui->page_local);
		break;
	case AddExisting:
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
			ui->lineEdit_remote_url->setFocus();
			break;
		case Initialize:
			ui->stackedWidget->setCurrentWidget(ui->page_local);
			browseLocalPath();
			break;
		case AddExisting:
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
		QString dir = ui->lineEdit_local_path->text();
		if (global->mainwindow->addExistingLocalRepository(dir, true)) {
			done(QDialog::Accepted);
		}
		return;
	}
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

void AddRepositoryDialog::on_pushButton_prev_clicked()
{
	auto *currpage = ui->stackedWidget->currentWidget();
	if (currpage == ui->page_local && mode() == Clone) {
		ui->stackedWidget->setCurrentWidget(ui->page_remote);
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

	QString path = defaultWorkingDir() / reponame_;
	path = misc::normalizePathSeparator(path);
	ui->lineEdit_local_path->setText(path);
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

void AddRepositoryDialog::complementRemoteURL(bool toggle)
{
	QString url = ui->lineEdit_remote_url->text();
	if (toggle && url.startsWith("https://github.com/")) {
		url = "git@github.com:" + url.mid(19);
		ui->lineEdit_remote_url->setText(url);
	} else if (toggle && url.startsWith("git@github.com:")) {
		url = "https://github.com/" + url.mid(15);
		ui->lineEdit_remote_url->setText(url);
	} else {
		QStringList s = misc::splitWords(url);
		if (s.size() == 3 && s[0] == "github") {
			QString url = "https://github.com/" + s[1] + '/' + s[2] + ".git";
			ui->lineEdit_remote_url->setText(url);
		}
	}
}
