#include "RepositoriesTreeWidget.h"
#include <QDropEvent>
#include <QDebug>
#include <QMimeData>

#include "MainWindow.h"

RepositoriesTreeWidget::RepositoriesTreeWidget(QWidget *parent)
	: QTreeWidget(parent)
{
}

MainWindow *RepositoriesTreeWidget::mainwindow()
{
	return qobject_cast<MainWindow *>(window());
}

void RepositoriesTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
	qDebug() << event;
	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
		event->accept();
		return;
	}
	QTreeWidget::dragEnterEvent(event);
}

void RepositoriesTreeWidget::dropEvent(QDropEvent *event)
{
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
			if (path.startsWith("file:///")) {
				path = path.mid(8);
				mainwindow()->addWorkingCopyDir(path);
			}
		}
	} else {
		QTreeWidget::dropEvent(event);
		emit dropped();
	}
}

