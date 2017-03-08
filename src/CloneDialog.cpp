#include "CloneDialog.h"
#include "ui_CloneDialog.h"
#include "misc.h"
#include "joinpath.h"
#include "MainWindow.h"
#include "SearchFromGitHubDialog.h"

#include <QMessageBox>
#include <QThread>

enum SearchRepository {
	None,
	GitHub,
};

struct CloneDialog::Private {
	MainWindow *mainwindow;
	QString url;
	QString default_working_dir;
};

CloneDialog::CloneDialog(QWidget *parent, const QString &url, const QString &defworkdir) :
	QDialog(parent),
	ui(new Ui::CloneDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	pv = new Private();

	pv->mainwindow = qobject_cast<MainWindow *>(parent);

	ui->lineEdit_repo_location->setText(url);
	pv->default_working_dir = defworkdir;
	ui->lineEdit_working_dir->setText(pv->default_working_dir);

	ui->comboBox->addItem(tr("Search"));
	ui->comboBox->addItem(tr("GitHub"));
}

CloneDialog::~CloneDialog()
{
	delete pv;
	delete ui;
}

MainWindow *CloneDialog::mainwindow()
{
	return pv->mainwindow;
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
	path = pv->default_working_dir / path;
	path = misc::normalizePathSeparator(path);
	ui->lineEdit_working_dir->setText(path);
}

void CloneDialog::on_pushButton_test_clicked()
{
	mainwindow()->testRemoteRepositoryValidity(url());
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
