#include "ObjectBrowserDialog.h"
#include "ui_ObjectBrowserDialog.h"

ObjectBrowserDialog::ObjectBrowserDialog(QWidget *parent, const QStringList &list)
	: QDialog(parent)
	, ui(new Ui::ObjectBrowserDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	for (QString const &text : list) {
		ui->listWidget->addItem(text);
	}
}

ObjectBrowserDialog::~ObjectBrowserDialog()
{
	delete ui;
}

QString ObjectBrowserDialog::text() const
{
	int row = ui->listWidget->currentRow();
	auto *item = ui->listWidget->item(row);
	if (item) {
		return item->text();
	}
	return QString();
}

void ObjectBrowserDialog::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
	done(QDialog::Accepted);
}
