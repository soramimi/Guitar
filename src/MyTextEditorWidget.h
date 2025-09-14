#ifndef MYTEXTEDITORWIDGET_H
#define MYTEXTEDITORWIDGET_H

#include "texteditor/TextEditorWidget.h"
#include "MyObjectViewBase.h"

class MainWindow;

class MyTextEditorWidget : public MyObjectViewBase, public TextEditorView {
protected:
	void contextMenuEvent(QContextMenuEvent *event) override;
public:
	MyTextEditorWidget(QWidget *parent = nullptr);
	void setDocument(const QList<Document::Line> *source, QString const &object_id, QString const &object_path);
	TextEditorView *view();
	void clear();
};

#endif // MYTEXTEDITORWIDGET_H
