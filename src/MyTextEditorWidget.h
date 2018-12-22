#ifndef MYTEXTEDITORWIDGET_H
#define MYTEXTEDITORWIDGET_H

#include "texteditor/TextEditorWidget.h"

class BasicMainWindow;

class MyTextEditorWidget : public TextEditorWidget {
private:
	BasicMainWindow *mainwindow;
	QString object_id;
	QString object_path;
public:
	MyTextEditorWidget(QWidget *parent = nullptr);
	void setDocument(const QList<Document::Line> *source, BasicMainWindow *mw, QString const &object_id, QString const &object_path);
protected:
	void contextMenuEvent(QContextMenuEvent *event) override;
};

#endif // MYTEXTEDITORWIDGET_H
