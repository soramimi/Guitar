#include "JumpDialog.h"
#include "ui_JumpDialog.h"
#include "common/joinpath.h"
#include "common/misc.h"
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

//	m->items = items;
	for (NamedCommitItem const &item : items) {
		NamedCommitItem newitem = item;
		QString name = newitem.name;
		QString remote = newitem.remote;
		if (!remote.isEmpty()) {
			newitem.name = "remotes" / remote / name;
		}
		m->items.push_back(newitem);
	}
	sort(&m->items);

	QStringList header = {
		tr("Name"),
	};

	ui->tableWidget->setColumnCount(1);
	{
		for (int i = 0; i < header.size(); i++) {
			auto *p = new QTableWidgetItem();
			p->setText(header[i]);
			ui->tableWidget->setHorizontalHeaderItem(i, p);
		}
	}
	updateTable();

	ui->tabWidget->setCurrentWidget(ui->tab_branches_tags);
	ui->lineEdit_filter->setFocus();
}

JumpDialog::~JumpDialog()
{
	delete m;
	delete ui;
}

JumpDialog::Action JumpDialog::action() const
{
	QWidget *w = ui->tabWidget->currentWidget();
	if (w == ui->tab_branches_tags) return Action::BranchsAndTags;
	if (w == ui->tab_find_text)     return Action::CommitId;
	return Action::None;
}

QString JumpDialog::text() const
{
	JumpDialog::Action a = action();

	if (a == JumpDialog::Action::BranchsAndTags) {
		return m->selected_name;
	}

	if (a == JumpDialog::Action::CommitId) {
		return ui->lineEdit_text->text();
	}

	return QString();
}

void JumpDialog::sort(NamedCommitList *items)
{
	std::sort(items->begin(), items->end(), [](NamedCommitItem const &l, NamedCommitItem const &r){
		auto Compare = [](NamedCommitItem const &l, NamedCommitItem const &r){
			int i;
			i = l.remote.compare(r.remote, Qt::CaseInsensitive);
			if (i == 0) {
				i = l.name.compare(r.name, Qt::CaseInsensitive);
			}
			return i;
		};
		return Compare(l, r) < 0;
	});
}

void JumpDialog::internalUpdateTable(NamedCommitList const &list)
{
	ui->tableWidget->clearContents();
	ui->tableWidget->setRowCount(list.size());
	for (int i = 0; i < list.size(); i++) {
		auto *p = new QTableWidgetItem();
		p->setText(list[i].name);
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
		internalUpdateTable(m->items);
	} else {
		QStringList filter = misc::splitWords(m->filter_text);
		NamedCommitList list;
		for (NamedCommitItem const &item: m->items) {
			auto Match = [&](QString  const &name){
				for (QString s : filter) {
					if (name.indexOf(s, 0, Qt::CaseInsensitive) < 0) {
						return false;
					}
				}
				return true;
			};
			if (!Match(item.name)) continue;
			list.push_back(item);
		}
		internalUpdateTable(list);
	}
}

void JumpDialog::on_lineEdit_filter_textChanged(QString const &text)
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

void JumpDialog::on_tabWidget_currentChanged(int /*index*/)
{
	if (ui->tabWidget->currentWidget() == ui->tab_branches_tags) {
		ui->checkBox_checkout->setVisible(true);
	} else {
		ui->checkBox_checkout->setVisible(false);
	}
}

