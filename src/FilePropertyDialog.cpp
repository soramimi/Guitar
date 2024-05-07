#include "FilePropertyDialog.h"
#include "ui_FilePropertyDialog.h"
#include "ApplicationGlobal.h"
#include "MainWindow.h"

FilePropertyDialog::FilePropertyDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::FilePropertyDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);
}

FilePropertyDialog::~FilePropertyDialog()
{
	delete ui;
}

void FilePropertyDialog::exec(QString const &path, QString const &id)
{
	ui->lineEdit_repo->setText(global->mainwindow->currentRepositoryName());
	ui->lineEdit_path->setText(path);
	ui->lineEdit_id->setText(id);

	QDialog::exec();
}
