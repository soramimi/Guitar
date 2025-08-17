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

void FilePropertyDialog::exec(QString const &path, GitHash const &id)
{
	QFileInfo info(path);
	if (!info.exists()) {
		QMessageBox::warning(this, tr("Error"), tr("File not found:\n\n%1").arg(path));
		return;
	}
	QByteArray ba;
	if (id) {
		GitObject obj = global->mainwindow->catFile(global->mainwindow->git(), id.toQString());
		if (obj.type == GitObject::Type::BLOB) {
			ba = obj.content;
		}
	} else {
		QFile file(path);
		if (file.open(QFile::ReadOnly)) {
			ba = file.read(1024 * 1024);
		}
	}
	std::string mimetype = global->filetype.file(ba.data(), ba.size()).mimetype;
	ui->lineEdit_repo->setText(global->mainwindow->currentRepositoryName());
	ui->lineEdit_path->setText(path);
	ui->lineEdit_type->setText(QString::fromStdString(mimetype));
	ui->lineEdit_id->setText(id.toQString());

	QDialog::exec();
}
