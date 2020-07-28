#include "SubmoduleAddDialog.h"
#include "ui_SubmoduleAddDialog.h"
#include "ApplicationGlobal.h"
#include "BasicMainWindow.h"
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

SubmoduleAddDialog::SubmoduleAddDialog(BasicMainWindow *parent, QString const &url, QString const &defworkdir, Git::Context const *gcx)
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
	ui->lineEdit_repo_location->setText(url);

	ui->advanced_option->setSshKeyOverrigingEnabled(!gcx->ssh_command.isEmpty());

#ifdef Q_OS_MACX
	ui->comboBox->setMinimumWidth(100);
#endif
}

SubmoduleAddDialog::~SubmoduleAddDialog()
{
	delete m;
	delete ui;
}

BasicMainWindow *SubmoduleAddDialog::mainwindow()
{
	return qobject_cast<BasicMainWindow *>(parent());
}

QString SubmoduleAddDialog::url()
{
	return ui->lineEdit_repo_location->text();
}

QString SubmoduleAddDialog::dir()
{
	return ui->lineEdit_working_dir->text();
}

void SubmoduleAddDialog::on_lineEdit_repo_location_textChanged(QString const &text)
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
		GitPtr g = mainwindow()->git(dir);
		QList<Git::Remote> vec;
		if (g->isValidWorkingCopy()) {
			g->getRemoteURLs(&vec);
		}
		for (Git::Remote const &r : vec) {
			if (r.purpose == "push" || url.isEmpty()) {
				url = r.url;
			}
		}
		ui->lineEdit_repo_location->setText(url);
		ui->lineEdit_working_dir->setText(dir);
		done(Accepted);
	}
}

QString SubmoduleAddDialog::overridedSshKey() const
{
	return ui->advanced_option->sshKey();
}




