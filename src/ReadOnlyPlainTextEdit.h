#ifndef READONLYPLAINTEXTEDIT_H
#define READONLYPLAINTEXTEDIT_H

#include <QPlainTextEdit>

class ReadOnlyPlainTextEdit : public QPlainTextEdit {
	Q_OBJECT
protected:
	void mousePressEvent(QMouseEvent *event) override;
public:
	explicit ReadOnlyPlainTextEdit(QWidget *parent = nullptr);
};

#endif // READONLYPLAINTEXTEDIT_H
