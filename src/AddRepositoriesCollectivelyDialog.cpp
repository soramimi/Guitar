#include "AddRepositoriesCollectivelyDialog.h"
#include "ui_AddRepositoriesCollectivelyDialog.h"

AddRepositoriesCollectivelyDialog::AddRepositoriesCollectivelyDialog(QWidget *parent, QStringList const &dirs)
	: QDialog(parent)
	, ui(new Ui::AddRepositoriesCollectivelyDialog)
{
	ui->setupUi(this);

	QStringList list = dirs;
	std::sort(list.begin(), list.end());

	for (QString const &dir : list) {
		QListWidgetItem *item = new QListWidgetItem(dir);
		item->setCheckState(Qt::Checked);
		ui->listWidget->addItem(item);
	}
}

AddRepositoriesCollectivelyDialog::~AddRepositoriesCollectivelyDialog()
{
	delete ui;
}

QStringList AddRepositoriesCollectivelyDialog::selectedDirs() const
{
	QStringList dirs;
	for (int i = 0; i < ui->listWidget->count(); i++) {
		QListWidgetItem *item = ui->listWidget->item(i);
		if (item->checkState() == Qt::Checked) {
			dirs.append(item->text());
		}
	}
	return dirs;
}
