#include "RepositoryPropertyDialog.h"
#include "ui_RepositoryPropertyDialog.h"
#include "misc.h"

RepositoryPropertyDialog::RepositoryPropertyDialog(QWidget *parent, const RepositoryItem &item) :
	QDialog(parent),
	ui(new Ui::RepositoryPropertyDialog)
{
	ui->setupUi(this);

	ui->label_name->setText(item.name);
	ui->lineEdit_local_dir->setText(misc::normalizePathSeparator(item.local_dir));
}

RepositoryPropertyDialog::~RepositoryPropertyDialog()
{
	delete ui;
}
