#include "MyTableWidgetDelegate.h"
#include "RemoteRepositoriesTableWidget.h"
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMenu>

RemoteRepositoriesTableWidget::RemoteRepositoriesTableWidget(QWidget *parent)
	: QTableWidget(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

	verticalHeader()->setVisible(false);

	setItemDelegate(new MyTableWidgetDelegate(this));
}

void RemoteRepositoriesTableWidget::contextMenuEvent(QContextMenuEvent *)
{
	int row = currentRow();
	QMenu menu;
	QAction *a_copy_url = menu.addAction(tr("Copy URL"));
	QAction *a = menu.exec(QCursor::pos() + QPoint(8, -8));
	if (a) {
		if (a == a_copy_url) {
			QString url = item(row, 1)->text();
			qApp->clipboard()->setText(url);
			return;
		}
	}
}


