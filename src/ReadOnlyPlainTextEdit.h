#ifndef READONLYPLAINTEXTEDIT_H
#define READONLYPLAINTEXTEDIT_H

#include <QPlainTextEdit>

class ReadOnlyPlainTextEdit : public QPlainTextEdit
{
	Q_OBJECT
public:
	explicit ReadOnlyPlainTextEdit(QWidget *parent = 0);

signals:

public slots:

protected:
	void mousePressEvent(QMouseEvent *event);
};

#endif // READONLYPLAINTEXTEDIT_H
