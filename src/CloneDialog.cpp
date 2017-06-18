#include "CloneDialog.h"
#include "ui_CloneDialog.h"
#include "misc.h"
#include "joinpath.h"
#include "MainWindow.h"
#include "SearchFromGitHubDialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QThread>

enum SearchRepository {
	None,
	GitHub,
};

struct CloneDialog::Private {
	MainWindow *mainwindow;
	QString url;
	QString repo_name;
	QString default_working_dir;
	bool ok = false;
	QString errmsg;
};

CloneDialog::CloneDialog(QWidget *parent, const QString &url, const QString &defworkdir)
	: QDialog(parent)
	, ui(new Ui::CloneDialog)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);


	m->mainwindow = qobject_cast<MainWindow *>(parent);

	ui->lineEdit_repo_location->setText(url);
	m->default_working_dir = defworkdir;
	ui->lineEdit_working_dir->setText(m->default_working_dir);

	ui->comboBox->addItem(tr("Search"));
	ui->comboBox->addItem(tr("GitHub"));
}

CloneDialog::~CloneDialog()
{
	delete m;
	delete ui;
}

MainWindow *CloneDialog::mainwindow()
{
	return m->mainwindow;
}

QString CloneDialog::url()
{
	return ui->lineEdit_repo_location->text();
}

QString CloneDialog::dir()
{
	return ui->lineEdit_working_dir->text();
}

void CloneDialog::on_lineEdit_repo_location_textChanged(const QString &text)
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
		SearchFromGitHubDialog dlg(this);
		if (dlg.exec() == QDialog::Accepted) {
			ui->lineEdit_repo_location->setText(dlg.url());
		}
	}
	ui->comboBox->setCurrentIndex(0);
}

void CloneDialog::on_pushButton_test_clicked()
{
	mainwindow()->testRemoteRepositoryValidity(url());
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
