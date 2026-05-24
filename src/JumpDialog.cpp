#include "JumpDialog.h"
#include "ui_JumpDialog.h"
#include "MyTableWidgetDelegate.h"
#include "common/joinpath.h"
#include "common/misc.h"

struct JumpDialog::Private {
	MyTableWidgetDelegate delegate;
	QString filter_text;
	QString selected_name;
	NamedCommitList items;
	CommitRecords commit_records;
	QStringList header;
};

JumpDialog::JumpDialog(QWidget *parent, const NamedCommitList &items, CommitRecords commit_records)
	: QDialog(parent)
	, ui(new Ui::JumpDialog)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	m->commit_records = commit_records;

	ui->tableWidget->setItemDelegate(&m->delegate);

	for (NamedCommitItem const &item : items) {
		NamedCommitItem newitem = item;
		if (newitem.type == NamedCommitItem::Type::BranchRemote) {
			if (!newitem.remote.empty()) {
				newitem.name = "remotes" / newitem.remote / newitem.name;
			}
		} else if (newitem.type == NamedCommitItem::Type::Tag) {
			newitem.name = "tags" / newitem.name;
		}
		m->items.push_back(newitem);
	}
	sort(&m->items);

	m->header = QStringList{
		tr("Name"),
		tr("Date"),
		tr("Message"),
	};

	ui->tableWidget->setColumnCount(m->header.size());
	{
		for (int i = 0; i < m->header.size(); i++) {
			auto *p = new QTableWidgetItem();
			p->setText(m->header[i]);
			ui->tableWidget->setHorizontalHeaderItem(i, p);
		}
	}
	updateTable();

	ui->lineEdit_filter->setFocus();
}

JumpDialog::~JumpDialog()
{
	delete m;
	delete ui;
}

QString JumpDialog::text() const
{
	int row = ui->tableWidget->currentRow();
	if (row < 0) {
		return ui->lineEdit_filter->text();
	} else {
		auto *item = ui->tableWidget->item(row, 0);
		if (item) {
			return item->text();
		}
	}
	return QString();
}

void JumpDialog::sort(NamedCommitList *items)
{
	std::sort(items->begin(), items->end(), [](NamedCommitItem const &l, NamedCommitItem const &r){
		auto Compare = [](NamedCommitItem const &l, NamedCommitItem const &r){
			if (l.type < r.type) return -1;
			if (l.type > r.type) return 1;
			if (l.type == NamedCommitItem::Type::BranchRemote) {
				int i = QString::fromStdString(l.remote).compare(QString::fromStdString(r.remote), Qt::CaseInsensitive);
				if (i != 0) return i;
			}
			return QString::fromStdString(l.name).compare(QString::fromStdString(r.name), Qt::CaseInsensitive);
		};
		return Compare(l, r) < 0;
	});
}

void JumpDialog::updateTable()
{
	auto InternalUpdateTable = [&](NamedCommitList const &list) {
		ui->tableWidget->clearContents();
		ui->tableWidget->setRowCount(list.size());
		for (int i = 0; i < list.size(); i++) {
			CommitRecord const *r = m->commit_records.find(list[i].id.toString());
			for (int col = 0; col < m->header.size(); col++) {
				auto *item = new QTableWidgetItem();
				QString text;
				switch (col)    {
				case 0: // Name
					text = QString::fromStdString(list[i].name);
					break;
				case 1: // Date
					if (r) {
						text = r->datetime;
					}
					break;
				case 2: // Message
					if (r) {
						text = r->message;
					}
					break;
				}
				item->setText(text);
				ui->tableWidget->setItem(i, col, item);
			}
		}
		ui->tableWidget->resizeColumnsToContents();
		ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
	};

	if (m->filter_text.isEmpty()) {
		InternalUpdateTable(m->items);
	} else {
		QStringList filter = misc::splitWords(m->filter_text);
		NamedCommitList list;
		for (NamedCommitItem const &item: m->items) {
			auto Match = [&](QString  const &name){
				for (QString const &s : filter) {
					if (name.indexOf(s, 0, Qt::CaseInsensitive) < 0) {
						return false;
					}
				}
				return true;
			};
			if (!Match(QString::fromStdString(item.name))) continue;
			list.push_back(item);
		}
		InternalUpdateTable(list);
	}
}

void JumpDialog::on_lineEdit_filter_textChanged(QString const &text)
{
	m->filter_text = text;
	updateTable();
}

void JumpDialog::on_tableWidget_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	int row = ui->tableWidget->currentRow();
	QTableWidgetItem *p = ui->tableWidget->item(row, 0);
	m->selected_name = p ? p->text() : QString();
}

void JumpDialog::on_pushButton_checkout_clicked()
{

}

