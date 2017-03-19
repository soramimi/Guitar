#include "RepositoryPropertyDialog.h"
#include "ui_RepositoryPropertyDialog.h"
#include "misc.h"
#include "MainWindow.h"

#include <QClipboard>
#include <QMenu>




RepositoryPropertyDialog::RepositoryPropertyDialog(MainWindow *parent, GitPtr g, const RepositoryItem &item)
	: BasicRepositoryDialog(parent, g)
	, ui(new Ui::RepositoryPropertyDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->label_name->setText(item.name);
	ui->lineEdit_local_dir->setText(misc::normalizePathSeparator(item.local_dir));

	updateRemotesTable(ui->tableWidget);
}

RepositoryPropertyDialog::~RepositoryPropertyDialog()
{
	delete ui;
}


