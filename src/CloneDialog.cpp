#include "CloneDialog.h"
#include "ui_CloneDialog.h"
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

struct CloneDialog::Private {
	QString url;
	QString repo_name;
	QString default_working_dir;
	bool ok = false;
	QString errmsg;
	CloneDialog::Action action = CloneDialog::Action::Clone;
};

CloneDialog::CloneDialog(MainWindow *parent, QString const &url, QString const &defworkdir, Git::Context const *gcx)
	: QDialog(parent)
	, ui(new Ui::CloneDialog)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	m->default_working_dir = defworkdir;
	ui->lineEdit_working_dir->setText(m->default_working_dir);
	ui->lineEdit_repo_location->setText(url);

	ui->comboBox->addItem(tr("Search"));
	ui->comboBox->addItem(tr("GitHub"));

	ui->advanced_option->setSshKeyOverrigingEnabled(!gcx->ssh_command.isEmpty());

#ifdef Q_OS_MACX
	ui->comboBox->setMinimumWidth(100);
#endif

	ui->lineEdit_repo_location->setFocus();
}

CloneDialog::~CloneDialog()
{
	delete m;
	delete ui;
}

CloneDialog::Action CloneDialog::action() const
{
	return m->action;
}

MainWindow *CloneDialog::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
}

QString CloneDialog::url()
{
	return ui->lineEdit_repo_location->text();
}

QString CloneDialog::dir()
{
	return ui->lineEdit_working_dir->text();
}

void CloneDialog::on_lineEdit_repo_location_textChanged(QString const &text)
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

void CloneDialog::on_comboBox_currentIndexChanged(int index)
{
	if (index == GitHub) {
		SearchFromGitHubDialog dlg(this, mainwindow());
		if (dlg.exec() == QDialog::Accepted) {
			ui->lineEdit_repo_location->setText(dlg.url());
		}
	}
	ui->comboBox->setCurrentIndex(0);
}

void CloneDialog::on_pushButton_test_clicked()
{
	mainwindow()->testRemoteRepositoryValidity(url(), overridedSshKey());
}

void CloneDialog::on_pushButton_browse_clicked()
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

void CloneDialog::on_pushButton_open_existing_clicked()
{
	QString dir = mainwindow()->defaultWorkingDir();
	dir = QFileDialog::getExistingDirectory(this, tr("Open existing directory"), dir);
	if (QFileInfo(dir).isDir()) {
		QString url;
		GitPtr g = mainwindow()->git(dir, {}, {});
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
		m->action = CloneDialog::Action::AddExisting;
		done(Accepted);
	}
}

QString CloneDialog::overridedSshKey() const
{
	return ui->advanced_option->sshKey();
}




