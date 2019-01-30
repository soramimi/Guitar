#include "BasicMainWindow.h"
#include "ObjectBrowserDialog.h"
#include "ui_ObjectBrowserDialog.h"
#include <QMessageBox>

enum {
	IdRole = Qt::UserRole,
	TypeRole,
};

ObjectBrowserDialog::ObjectBrowserDialog(BasicMainWindow *parent, const QStringList &list)
	: QDialog(parent)
	, ui(new Ui::ObjectBrowserDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	GitPtr g = git();

	QStringList cols = {
		tr("ID"),
		tr("Type"),
	};

	auto NewQTableWidgetItem = [](){
		QTableWidgetItem *item = new QTableWidgetItem();
		return item;
	};

	ui->tableWidget->setColumnCount(cols.size());
	for (int i = 0; i < cols.size(); i++) {
		QTableWidgetItem *item = NewQTableWidgetItem();
		item->setText(cols[i]);
		ui->tableWidget->setHorizontalHeaderItem(i, item);
	}
	ui->tableWidget->setRowCount(list.size());

	for (int row = 0; row < list.size(); row++) {
		QString const &text = list[row];
		QString type = g->objectType(text);
		auto *item0 = NewQTableWidgetItem();
		item0->setData(IdRole, text);
		item0->setData(TypeRole, type);
		item0->setText(text);
		ui->tableWidget->setItem(row, 0, item0);
		auto *item1 = NewQTableWidgetItem();
		item1->setText(type);
		ui->tableWidget->setItem(row, 1, item1);
	}
	ui->tableWidget->resizeColumnsToContents();

	ui->tableWidget->setCurrentItem(ui->tableWidget->item(0, 0));
}

ObjectBrowserDialog::~ObjectBrowserDialog()
{
	delete ui;
}

BasicMainWindow *ObjectBrowserDialog::mainwindow()
{
	return qobject_cast<BasicMainWindow *>(parent());
}

GitPtr ObjectBrowserDialog::git()
{
	return mainwindow()->git();
}

QString ObjectBrowserDialog::text() const
{
	int row = ui->tableWidget->currentRow();
	auto *item = ui->tableWidget->item(row, 0);
	if (item) {
		return item->data(IdRole).toString();
	}
	return QString();
}

void ObjectBrowserDialog::on_tableWidget_itemDoubleClicked(QTableWidgetItem *item)
{
	(void)item;
	done(QDialog::Accepted);
}

void ObjectBrowserDialog::on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous)
{
	(void)current;
	(void)previous;

//	if (current) {
//		QString id = current->data(IdRole).toString();
//		QString ty = current->data(TypeRole).toString();
//		if (Git::isValidID(id) && ty == "commit") {




//		}
//	}
}

void ObjectBrowserDialog::on_pushButton_inspect_clicked()
{
	int row = ui->tableWidget->currentRow();
	auto *item = ui->tableWidget->item(row, 0);
	if (item) {
		GitPtr g = git();
		QString id = item->data(IdRole).toString();
		QString ty = item->data(TypeRole).toString();
		if (Git::isValidID(id) && ty == "commit") {
			Git::CommitItem commit;
			if (g->queryCommit(id, &commit)) {
				mainwindow()->execCommitPropertyDialog(this, &commit);
			}
		} else {
			QMessageBox::information(this, tr("Object Inspection"), id + "\n\n" + ty);
		}
	}
}


