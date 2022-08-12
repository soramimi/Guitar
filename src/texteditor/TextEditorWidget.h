#ifndef TEXTEDITORWIDGET_H
#define TEXTEDITORWIDGET_H

#include "TextEditorView.h"
#include <QWidget>
#include <QScrollBar>
#include <QGridLayout>

class TextEditorWidget : public QWidget {
	Q_OBJECT
private:
	QGridLayout *layout_;
	TextEditorView *view_;
	QScrollBar *vsb_;
	QScrollBar *hsb_;
public:
	explicit TextEditorWidget(QWidget *parent = nullptr);
	TextEditorView *view()
	{
		return view_;
	}
	void updateLayout()
	{
		view_->updateLayout();
	}
	void updateView()
	{
		view_->updateView();
	}
};

#endif // TEXTEDITORWIDGET_H

