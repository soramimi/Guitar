#include "MyImageViewWidget.h"

#include <QMenu>
#include <QFileDialog>

#include "common/misc.h"
#include "common/joinpath.h"

MyImageViewWidget::MyImageViewWidget(QWidget *parent)
	: ImageViewWidget(parent)
{

}

void MyImageViewWidget::setImage(QString mimetype, QByteArray const &ba, QString const &object_id, QString const &path)
{
	this->object_id_ = object_id;
	this->path_ = path;
	ImageViewWidget::setImage(mimetype, ba);
}

void MyImageViewWidget::contextMenuEvent(QContextMenuEvent *e)
{
	QString id = object_id_;
	if (id.startsWith(PATH_PREFIX)) {
		// pass
	} else if (Git::isValidID(id)) {
		// pass
	} else {
		return; // invalid id
	}

	QMenu menu;

	QAction *a_save_as = id.isEmpty() ? nullptr : menu.addAction(tr("Save as..."));
	if (!menu.actions().isEmpty()) {
		update();
		QAction *a = menu.exec(misc::contextMenuPos(this, e));
		if (a) {
			if (a == a_save_as) {
				QString path = mainwindow()->currentWorkingCopyDir() / path_;
				QString dstpath = QFileDialog::getSaveFileName(window(), tr("Save as"), path);
				if (!dstpath.isEmpty()) {
					mainwindow()->saveAs(id, dstpath);
				}
				update();
				return;
			}
		}
	}
}

