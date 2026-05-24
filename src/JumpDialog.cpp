#include "JumpDialog.h"
#include "ui_JumpDialog.h"
#include "MyTableWidgetDelegate.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include "ApplicationGlobal.h"
#include <MainWindow.h>
#include <QKeyEvent>
#include <optional>

struct JumpDialog::Private {
	MyTableWidgetDelegate delegate;
	QString filter_text;
	QString selected_name;

	NamedCommitList all_items;
	std::optional<NamedCommitList> filtered_items;

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

	qApp->installEventFilter(this);
	installEventFilter(this);

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
		m->all_items.push_back(newitem);
	}
	sort(&m->all_items);

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

MainWindow *JumpDialog::mainwindow()
{
	return global->mainwindow;
}

bool JumpDialog::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->tableWidget) {
		if (event->type() == QEvent::FocusIn) {
			int row = ui->tableWidget->currentRow();
			if (row < 0) {
				row = 0;
			}
			ui->tableWidget->setCurrentCell(row, 0);
			ui->tableWidget->selectRow(row);
			return true;
		}
	}
	return false;
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

CommitRecord const *JumpDialog::currentCommit() const
{
	int i = ui->tableWidget->currentRow();
	return m->commit_records.find(m->all_items[i].id.toString());
}

NamedCommitItem const *JumpDialog::currentItem() const
{
	NamedCommitList const *list = m->filtered_items ? &*m->filtered_items : &m->all_items;

	int i = ui->tableWidget->currentRow();
	if (i < 0) {
		i = 0;
	}
	if (i < list->size()) {
		return &list->at(i);
	}
	return nullptr;
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

	m->filtered_items.reset();

	if (m->filter_text.isEmpty()) {
		InternalUpdateTable(m->all_items);
	} else {
		m->filtered_items = NamedCommitList();
		QStringList filter = misc::splitWords(m->filter_text);
		for (NamedCommitItem const &item: m->all_items) {
			auto Match = [&](QString  const &name){
				for (QString const &s : filter) {
					if (name.indexOf(s, 0, Qt::CaseInsensitive) < 0) {
						return false;
					}
				}
				return true;
			};
			if (!Match(QString::fromStdString(item.name))) continue;
			m->filtered_items->push_back(item);
		}
		InternalUpdateTable(*m->filtered_items);
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
	NamedCommitItem const *item = currentItem();
	if (item) {
		std::string name = item->name;
		if (item->type == NamedCommitItem::Type::BranchRemote) {
			if (misc::starts_with(name, "remotes/")) {
				size_t i = 8;
				i = name.find('/', i);
				if (i != std::string::npos && i > 0) {
					name = name.substr(i + 1);
				}
			}
		} else if (item->type == NamedCommitItem::Type::Tag) {
			if (misc::starts_with(name, "tags/")) {
				name = name.substr(5);
			}
		}
		if (mainwindow()->checkoutLocalBranch(name)) {
			done(QDialog::Accepted);
		}
	}
}

