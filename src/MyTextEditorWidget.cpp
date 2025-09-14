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
	this->object_id_.clear();
	this->object_path_.clear();
	view()->clear();
	update();
}

void MyTextEditorWidget::setDocument(const QList<Document::Line> *source, QString const &object_id, QString const &object_path)
{
	this->object_id_ = object_id;
	this->object_path_ = object_path;
	view()->setDocument(source);
}

void MyTextEditorWidget::contextMenuEvent(QContextMenuEvent *event)
{
	if (!isValidObject()) return;

	QString id = object_id_;

	QMenu menu;

	QAction *a_save_as = menu.addAction(tr("Save as..."));
	QAction *a_copy = menu.addAction(tr("Copy"));
	if (!menu.actions().isEmpty()) {
		update();
		QAction *a = menu.exec(misc::contextMenuPos(this, event));
		if (a) {
			if (a == a_save_as) {
				if (saveAs(window())) {
					update();
				}
				return;
			}
			if (a == a_copy) {
				view()->editCopy();
				return;
			}
		}
	}
}
