#include "AddRepositoriesCollectivelyDialog.h"
#include "ui_AddRepositoriesCollectivelyDialog.h"
#include "ApplicationGlobal.h"

namespace {
QListWidgetItem *new_QListWidgetItem(QString const &text)
{
	auto *p = new QListWidgetItem(text);
	p->setSizeHint({20, 20});
	return p;
}
}

AddRepositoriesCollectivelyDialog::AddRepositoriesCollectivelyDialog(QWidget *parent, QStringList const &dirs)
	: QDialog(parent)
	, ui(new Ui::AddRepositoriesCollectivelyDialog)
{
	ui->setupUi(this);

	QStringList list = dirs;
	std::sort(list.begin(), list.end());

	for (QString const &dir : list) {
		QListWidgetItem *item = new_QListWidgetItem(dir);
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
