#include "JumpDialog.h"
#include "ui_JumpDialog.h"

#include <QDebug>

struct JumpDialog::Private {
	MyTableWidgetDelegate delegate;
	QString filter_text;
	QString selected_name;
	NamedCommitList items;
};

JumpDialog::JumpDialog(QWidget *parent, const NamedCommitList &items)
	: QDialog(parent)
	, ui(new Ui::JumpDialog)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->tableWidget->setItemDelegate(&m->delegate);

	m->items = items;

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
	delete m;
	delete ui;
}

QString JumpDialog::selectedName() const
{
	return m->selected_name;
}

void JumpDialog::sort(NamedCommitList *items)
{
	std::sort(items->begin(), items->end(), [](NamedCommitItem const &l, NamedCommitItem const &r){
		return l.name.compare(r.name, Qt::CaseInsensitive) < 0;
	});
}

void JumpDialog::updateTable_(NamedCommitList const &list)
{
	ui->tableWidget->clearContents();
	ui->tableWidget->setRowCount(list.size());
	for (int i = 0; i < list.size(); i++) {
		QTableWidgetItem *p = new QTableWidgetItem();
		QString name = list[i].name;
		if (!m->filter_text.isEmpty() && name.indexOf(m->filter_text) < 0) {
			continue;
		}
		p->setText(name);
		ui->tableWidget->setItem(i, 0, p);
		ui->tableWidget->setRowHeight(i, 24);
	}
	ui->tableWidget->resizeColumnsToContents();
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
	ui->tableWidget->setCurrentCell(0, 0);
}

void JumpDialog::updateTable()
{
	if (m->filter_text.isEmpty()) {
		updateTable_(m->items);
	} else {
		NamedCommitList list;
		for (NamedCommitItem const &item: m->items) {
			if (item.name.indexOf(m->filter_text) < 0) {
				continue;
			}
			list.push_back(item);
		}
		updateTable_(list);
	}
}

void JumpDialog::on_lineEdit_filter_textChanged(const QString &text)
{
	m->filter_text = text;
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
	m->selected_name = p ? p->text() : QString();
}

bool JumpDialog::isCheckoutChecked()
{
	return ui->checkBox_checkout->isChecked();
}






