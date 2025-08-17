#include "SubmoduleAddDialog.h"
#include "ui_SubmoduleAddDialog.h"
#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include "SearchFromGitHubDialog.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QThread>

enum SearchRepository {
	None,
	GitHub,
};

struct SubmoduleAddDialog::Private {
	QString url;
	QString repo_name;
	QString default_working_dir;
	bool ok = false;
	QString errmsg;
};

SubmoduleAddDialog::SubmoduleAddDialog(MainWindow *parent, QString const &url, QString const &defworkdir)
	: QDialog(parent)
	, ui(new Ui::SubmoduleAddDialog)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	m->default_working_dir = defworkdir;
	ui->lineEdit_working_dir->setText(m->default_working_dir);
	ui->lineEdit_remote->setText(url);

	ui->advanced_option->setSshKeyOverrigingEnabled(!global->appsettings.ssh_command.isEmpty());

	ui->lineEdit_remote->setFocus();
}

SubmoduleAddDialog::~SubmoduleAddDialog()
{
	delete m;
	delete ui;
}

MainWindow *SubmoduleAddDialog::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
}

QString SubmoduleAddDialog::url()
{
	return ui->lineEdit_remote->text();
}

QString SubmoduleAddDialog::dir()
{
	return ui->lineEdit_working_dir->text();
}

bool SubmoduleAddDialog::isForce() const
{
	return ui->checkBox_force->isChecked();
}


void SubmoduleAddDialog::on_lineEdit_remote_textChanged(const QString &text)
{
	QString path;
	int i = text.lastIndexOf('/');
	int j = text.lastIndexOf('\\');
	if (i < j) i = j;
	j = text.size();
	if (text.endsWith(".git")) {
		j -= 4;
	}
	if (i >= 0 && i < j) {
		path = text.mid(i, j - i);
	}
	m->repo_name = path;
	path = m->default_working_dir / m->repo_name;
	path = misc::normalizePathSeparator(path);
	ui->lineEdit_working_dir->setText(path);
}



void SubmoduleAddDialog::on_pushButton_test_clicked()
{
	mainwindow()->testRemoteRepositoryValidity(url(), overridedSshKey());
}

void SubmoduleAddDialog::on_pushButton_browse_clicked()
{
	QString path = ui->lineEdit_working_dir->text();
	path = QFileDialog::getExistingDirectory(this, tr("Checkout into"), path);
	if (!path.isEmpty()) {
		m->default_working_dir = path;
		path = m->default_working_dir / m->repo_name;
		path = misc::normalizePathSeparator(path);
		ui->lineEdit_working_dir->setText(path);
	}
}

void SubmoduleAddDialog::on_pushButton_open_existing_clicked()
{
	QString dir = mainwindow()->defaultWorkingDir();
	dir = QFileDialog::getExistingDirectory(this, tr("Open existing directory"), dir);
	if (QFileInfo(dir).isDir()) {
		QString url;
		GitRunner g = mainwindow()->new_git_runner(dir, {});
		std::vector<GitRemote> vec;
		if (g.isValidWorkingCopy()) {
			g.remote_v(&vec);
		}
		for (GitRemote const &r : vec) {
			url = r.url_fetch;
			if (!url.isEmpty()) break;
			url = r.url_push;
			if (!url.isEmpty()) break;
		}
		ui->lineEdit_remote->setText(url);
		ui->lineEdit_working_dir->setText(dir);
		done(Accepted);
	}
}

QString SubmoduleAddDialog::overridedSshKey() const
{
	return ui->advanced_option->sshKey();
}




