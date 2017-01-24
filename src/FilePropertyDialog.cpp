#include "FilePropertyDialog.h"
#include "MainWindow.h"
#include "ui_FilePropertyDialog.h"

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

void FilePropertyDialog::exec(MainWindow *mw, QString const &path, QString const &id)
{
	mainwindow = mw;

	ui->lineEdit_repo->setText(mainwindow->currentRepositoryName());
	ui->lineEdit_path->setText(path);
	ui->lineEdit_id->setText(id);

	QDialog::exec();
}
