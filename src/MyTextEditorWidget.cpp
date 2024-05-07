#include "MyTextEditorWidget.h"
#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include <QFileDialog>
#include <QFontDatabase>
#include <QMenu>

MyTextEditorWidget::MyTextEditorWidget(QWidget *parent)
	: TextEditorView(parent)
{
	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	view()->setTextFont(font);
}

TextEditorView *MyTextEditorWidget::view()
{
	return this;
}

void MyTextEditorWidget::clear()
{
	this->object_id.clear();
	this->object_path.clear();
	view()->clear();
	update();
}

void MyTextEditorWidget::setDocument(const QList<Document::Line> *source, QString const &object_id, QString const &object_path)
{
	this->object_id = object_id;
	this->object_path = object_path;
	view()->setDocument(source);
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
				QString path = global->mainwindow->currentWorkingCopyDir() / object_path;
				QString dstpath = QFileDialog::getSaveFileName(window(), tr("Save as"), path);
				if (!dstpath.isEmpty()) {
					global->mainwindow->saveAs(global->mainwindow->frame(), id, dstpath);
				}
				update();
				return;
			}
			if (a == a_copy) {
				view()->editCopy();
				return;
			}
		}
	}
}
