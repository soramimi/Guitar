#ifndef READONLYLINEEDIT_H
#define READONLYLINEEDIT_H

#include <QLineEdit>

class ReadOnlyLineEdit : public QLineEdit
{
public:
	ReadOnlyLineEdit(QWidget *parent = 0);

	// QWidget interface
protected:
	void paintEvent(QPaintEvent *event);
};

#endif // READONLYLINEEDIT_H
