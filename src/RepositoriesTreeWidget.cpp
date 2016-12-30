#include "RepositoriesTreeWidget.h"
#include <QDropEvent>
#include <QDebug>
#include <QMimeData>

RepositoriesTreeWidget::RepositoriesTreeWidget(QWidget *parent)
	: QTreeWidget(parent)
{

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

	QTreeWidget::dropEvent(event);
	emit dropped();
}
