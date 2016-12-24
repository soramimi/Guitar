#include "CloneDialog.h"
#include "ui_CloneDialog.h"
#include "misc.h"
#include "joinpath.h"

CloneDialog::CloneDialog(QWidget *parent, GitPtr gitptr, const QString &defworkdir) :
	QDialog(parent),
	ui(new Ui::CloneDialog)
{
	ui->setupUi(this);
	git = gitptr;
	default_working_dir = defworkdir;
	ui->lineEdit_working_dir->setText(default_working_dir);
}

CloneDialog::~CloneDialog()
{
	delete ui;
}

void CloneDialog::accept()
{
	QString loc = ui->lineEdit_repo_location->text();
	QString dir = ui->lineEdit_working_dir->text();
	git->clone(loc, dir);
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
	path = default_working_dir / path;
	path = misc::normalizePathSeparator(path);
	ui->lineEdit_working_dir->setText(path);
}
