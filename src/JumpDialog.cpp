#include "JumpDialog.h"
#include "ui_JumpDialog.h"

#include <QDebug>

JumpDialog::JumpDialog(QWidget *parent, const QList<Item> &list) :
	QDialog(parent),
	ui(new Ui::JumpDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->tableWidget->setItemDelegate(&delegate_);

	this->list = list;

	QStringList header = {
		tr("Name"),
	};

	ui->tableWidget->setColumnCount(header.size());
	{
		for (int i = 0; i < header.size(); i++) {
			QTableWidgetItem *p = new QTableWidgetItem();
			p->setText(header[i]);
			ui->tableWidget->setHorizontalHeaderItem(i, p);
		}
	}
	updateTable();
}

JumpDialog::~JumpDialog()
{
	delete ui;
}

QString JumpDialog::selectedName() const
{
	return selected_name;
}

void JumpDialog::sort(QList<JumpDialog::Item> *items)
{
	std::sort(items->begin(), items->end(), [](JumpDialog::Item const &l, JumpDialog::Item const &r){
		return l.name.compare(r.name, Qt::CaseInsensitive) < 0;
	});
}

void JumpDialog::updateTable_(QList<Item> const &list)
{
	ui->tableWidget->clearContents();
	ui->tableWidget->setRowCount(list.size());
	for (int i = 0; i < list.size(); i++) {
		QTableWidgetItem *p = new QTableWidgetItem();
		QString name = list[i].name;
		if (!filter_text.isEmpty() && name.indexOf(filter_text) < 0) {
			continue;
		}
		p->setText(name);
		ui->tableWidget->setItem(i, 0, p);
		ui->tableWidget->setRowHeight(i, 24);
	}
	ui->tableWidget->resizeColumnsToContents();
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
}

void JumpDialog::updateTable()
{
	if (filter_text.isEmpty()) {
		updateTable_(list);
	} else {
		QList<Item> list2;
		for (Item const &item: list) {
			if (item.name.indexOf(filter_text) < 0) {
				continue;
			}
			list2.push_back(item);
		}
		updateTable_(list2);
	}
}

void JumpDialog::on_lineEdit_filter_textChanged(const QString &text)
{
	filter_text = text;
	updateTable();
}

void JumpDialog::on_toolButton_clicked()
{
	ui->lineEdit_filter->clear();
	ui->lineEdit_filter->setFocus();
}

void JumpDialog::on_tableWidget_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	int row = ui->tableWidget->currentRow();
	QTableWidgetItem *p = ui->tableWidget->item(row, 0);
	selected_name = p ? p->text() : QString();
}

