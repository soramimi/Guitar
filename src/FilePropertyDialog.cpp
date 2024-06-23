#include "FilePropertyDialog.h"
#include "ui_FilePropertyDialog.h"
#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include <QMessageBox>

#include <QFile>
#include <QFileInfo>

FilePropertyDialog::FilePropertyDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::FilePropertyDialog)
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

void FilePropertyDialog::exec(QString const &path, Git::CommitID const &id)
{
	QFileInfo info(path);
	if (!info.exists()) {
		QMessageBox::warning(this, tr("Error"), tr("File not found:\n\n%1").arg(path));
		return;
	}
	QByteArray ba;
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		ba = file.read(1024 * 1024);
	}
	QString mimetype = global->filetype.mime_by_data(ba.data(), ba.size());
	ui->lineEdit_repo->setText(global->mainwindow->currentRepositoryName());
	ui->lineEdit_path->setText(path);
	ui->lineEdit_type->setText(mimetype);
	ui->lineEdit_id->setText(id.toQString());

	QDialog::exec();
}
