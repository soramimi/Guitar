#include "RepositoriesTreeWidget.h"
#include <QDropEvent>
#include <QDebug>
#include <QMimeData>
#include <QApplication>
#include "MainWindow.h"

RepositoriesTreeWidget::RepositoriesTreeWidget(QWidget *parent)
	: QTreeWidget(parent)
{
	connect(this, &RepositoriesTreeWidget::currentItemChanged, [&](QTreeWidgetItem *current, QTreeWidgetItem *previous){
		current_item = current;
	});
}

MainWindow *RepositoriesTreeWidget::mainwindow()
{
	return qobject_cast<MainWindow *>(window());
}

void RepositoriesTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if (QApplication::modalWindow()) return;

	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
		event->accept();
		return;
	}
	QTreeWidget::dragEnterEvent(event);
}

void RepositoriesTreeWidget::dropEvent(QDropEvent *event)
{
	if (QApplication::modalWindow()) return;

	if (0) {
		QMimeData const *mimedata = event->mimeData();
		QByteArray encoded = mimedata->data("application/x-qabstractitemmodeldatalist");
		QDataStream stream(&encoded, QIODevice::ReadOnly);
		while (!stream.atEnd()) {
			int row, col;
			QMap<int,  QVariant> roledatamap;
			stream >> row >> col >> roledatamap;
		}
	}

	if (event->mimeData()->hasUrls()) {
		Q_ASSERT(mainwindow());
		QList<QUrl> urls = event->mimeData()->urls();
		for (QUrl const &url : urls) {
			QString path = url.url();
			if (path.startsWith("file://")) {
				int i = 7;
#ifdef Q_OS_WIN
				if (path.utf16()[i] == '/') {
					i++;
				}
#endif
				path = path.mid(i);
				mainwindow()->addWorkingCopyDir(path, false);
			}
		}
	} else {
		QTreeWidgetItem *item = current_item;
		QTreeWidget::dropEvent(event);
		setCurrentItem(item);
		emit dropped();
	}
}

