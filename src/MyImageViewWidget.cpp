#include "MyImageViewWidget.h"

#include <QMenu>
#include <QFileDialog>
#include "common/joinpath.h"
#include "common/misc.h"

MyImageViewWidget::MyImageViewWidget(QWidget *parent)
	: ImageViewWidget(parent)
{

}

void MyImageViewWidget::setImage(QString const &mimetype, QByteArray const &ba, QString const &object_id, QString const &path)
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
				auto *mw = qobject_cast<MainWindow *>(mainwindow());
				QString path = mw->currentWorkingCopyDir() / path_;
				QString dstpath = QFileDialog::getSaveFileName(window(), tr("Save as"), path);
				if (!dstpath.isEmpty()) {
					mw->saveAs(id, dstpath);
				}
				update();
				return;
			}
		}
	}
}

