#include "MyImageViewWidget.h"
#include <QFileDialog>
#include <QMenu>
#include <common/joinpath.h>
#include <common/qmisc.h>

MyImageViewWidget::MyImageViewWidget(QWidget *parent)
	: ImageViewWidget(parent)
{
}

void MyImageViewWidget::setImage(std::string const &mimetype, QByteArray const &ba, std::string const &object_id, QString const &path)
{
	this->object_id_ = object_id;
	this->object_path_ = path;
	ImageViewWidget::setImage(mimetype, ba);
}

void MyImageViewWidget::contextMenuEvent(QContextMenuEvent *e)
{
	if (!isValidObject()) return;

	QMenu menu;

	QAction *a_save_as = menu.addAction(tr("Save as..."));
	if (!menu.actions().isEmpty()) {
		update();
		QAction *a = menu.exec(misc::contextMenuPos(this, e));
		if (a) {
			if (a == a_save_as) {
				if (saveAs(window())) {
					update();
				}
				return;
			}
		}
	}
}

