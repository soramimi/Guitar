#include "MyTextEditorWidget.h"
#include "MainWindow.h"
#include "common/misc.h"
#include <QMenu>
#include <QFileDialog>
#include "common/joinpath.h"

MyTextEditorWidget::MyTextEditorWidget(QWidget *parent)
	: TextEditorWidget(parent)
{

}

void MyTextEditorWidget::setDocument(const QList<Document::Line> *source, MainWindow *mw, QString const &object_id, QString const &object_path)
{
	this->mainwindow = mw;
	this->object_id = object_id;
	this->object_path = object_path;
	TextEditorWidget::setDocument(source);
}

void MyTextEditorWidget::contextMenuEvent(QContextMenuEvent *event)
{
	QString id = object_id;
	if (id.startsWith(PATH_PREFIX)) {
		// pass
	} else if (Git::isValidID(id)) {
		// pass
	} else {
		return; // invalid id
	}

	QMenu menu;

	QAction *a_save_as = id.isEmpty() ? nullptr : menu.addAction(tr("Save as..."));
	QAction *a_copy = menu.addAction(tr("Copy"));
	if (!menu.actions().isEmpty()) {
		update();
		QAction *a = menu.exec(misc::contextMenuPos(this, event));
		if (a) {
			if (a == a_save_as) {
				QString path = mainwindow->currentWorkingCopyDir() / object_path;
				QString dstpath = QFileDialog::getSaveFileName(window(), tr("Save as"), path);
				if (!dstpath.isEmpty()) {
					mainwindow->saveAs(id, dstpath);
				}
				update();
				return;
			}
			if (a == a_copy) {
				editCopy();
				return;
			}
		}
	}

}
