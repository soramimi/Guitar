#ifndef MYTEXTEDITORWIDGET_H
#define MYTEXTEDITORWIDGET_H

#include "texteditor/TextEditorWidget.h"

class MainWindow;

class MyTextEditorWidget : public TextEditorWidget {
private:
	MainWindow *mainwindow;
	QString object_id;
	QString object_path;
public:
	MyTextEditorWidget(QWidget *parent = 0);

	// QWidget interface
	void setDocument(const QList<Document::Line> *source, MainWindow *mw, QString const &object_id, QString const &object_path);
protected:
	void contextMenuEvent(QContextMenuEvent *event);
};

#endif // MYTEXTEDITORWIDGET_H
