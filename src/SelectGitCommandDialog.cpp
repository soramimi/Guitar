#include "SelectGitCommandDialog.h"
#include "ui_SelectGitCommandDialog.h"
#include <QFileDialog>

SelectGitCommandDialog::SelectGitCommandDialog(QWidget *parent, const QString &path, const QStringList &list) :
	QDialog(parent),
	ui(new Ui::SelectGitCommandDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);


	QString text = tr("Please select the '%1' command you want to use.");
#ifdef _WIN32
	text = text.arg("git.exe");
#else
	text = text.arg("git");
#endif
	ui->label->setText(text);

	this->path = path;

	for (QString const &s : list) {
		ui->listWidget->addItem(s);
	}

	ui->listWidget->setFocus();
	ui->listWidget->setCurrentRow(0);
}

SelectGitCommandDialog::~SelectGitCommandDialog()
{
	delete ui;
}


void SelectGitCommandDialog::on_pushButton_browse_clicked()
{
	QString dir;
#ifdef _WIN32
	QString filter = tr("Git command (git.exe);;Executable files (*.exe)");
#else
	QString filter = tr("Git command (git);;All files (*)");
#endif
	QFileDialog::Options opts;
	QFileDialog dlg(this);
	dlg.setWindowTitle(tr("git command"));
	dlg.setDirectory(dir);
	dlg.setNameFilter(filter);
	dlg.setFilter(QDir::Dirs | QDir::Executable);
	dlg.setFileMode(QFileDialog::ExistingFile);
	if (dlg.exec() == QDialog::Accepted) {
		QStringList list = dlg.selectedFiles();
		if (list.size() > 0) {
			path = list.first();
			accept();
		}
	}
//	path = QFileDialog::getOpenFileName(this, "git command", dir, filter, nullptr, opts);

}

void SelectGitCommandDialog::on_listWidget_currentTextChanged(const QString &currentText)
{
	path = currentText;
}

QString SelectGitCommandDialog::selectedFile() const
{
	return path;
}

