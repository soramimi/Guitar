#include "CloneDialog.h"
#include "ui_CloneDialog.h"
#include "misc.h"
#include "joinpath.h"
#include "MainWindow.h"

#include <QMessageBox>
#include <QThread>



struct CloneDialog::Private {
	MainWindow *mainwindow;
	GitPtr git;
	QString url;
	QString default_working_dir;
};

CloneDialog::CloneDialog(QWidget *parent, GitPtr gitptr, const QString &defworkdir) :
	QDialog(parent),
	ui(new Ui::CloneDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	pv = new Private();

	pv->mainwindow = qobject_cast<MainWindow *>(parent);

	pv->git = gitptr;
	pv->default_working_dir = defworkdir;
	ui->lineEdit_working_dir->setText(pv->default_working_dir);
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






