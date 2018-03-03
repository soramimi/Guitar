#include "SelectCommandDialog.h"
#include "ui_SelectCommandDialog.h"
#include <QFileDialog>

QStringList uniqueStringList(const QStringList &list)
{
	QStringList tmp_list = list;
	std::sort(tmp_list.begin(), tmp_list.end());
	auto end = std::unique(tmp_list.begin(), tmp_list.end());
	QStringList ret_list;
	for (auto it = tmp_list.begin(); it != end; it++) {
		ret_list.push_back(*it);
	}
	return ret_list;
}

SelectCommandDialog::SelectCommandDialog(QWidget *parent, const QString &cmdname, const QString &cmdfile, const QString &path, const QStringList &list) :
	QDialog(parent),
	ui(new Ui::SelectCommandDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	command_name = cmdname;
	command_file = cmdfile;

	QString text = tr("Please select the '%1' command you want to use.");
	ui->label->setText(text.arg(cmdfile));

	this->path = path;

	QStringList list2 = uniqueStringList(list);

	for (QString const &s : list2) {
		ui->listWidget->addItem(s);
	}

	ui->listWidget->setFocus();
	ui->listWidget->setCurrentRow(0);
}

SelectCommandDialog::~SelectCommandDialog()
{
	delete ui;
}


void SelectCommandDialog::on_pushButton_browse_clicked()
{
	QString dir;
#ifdef _WIN32
	QString filter = tr("%1 command (%2);;Executable files (*.exe)");
#else
	QString filter = tr("%1 command (%2);;All files (*)");
#endif
	filter = filter.arg(command_name).arg(command_file);

	QFileDialog dlg(this);
	dlg.setWindowTitle(tr("%1 command").arg(command_name));
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
}

void SelectCommandDialog::on_listWidget_currentTextChanged(const QString &currentText)
{
	path = currentText;
}

QString SelectCommandDialog::selectedFile() const
{
	return path;
}

void SelectCommandDialog::on_listWidget_itemDoubleClicked(QListWidgetItem * /*item*/)
{
	accept();
}
