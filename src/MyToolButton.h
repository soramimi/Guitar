#ifndef MYTOOLBUTTON_H
#define MYTOOLBUTTON_H

#include <QToolButton>

class MyToolButton : public QToolButton
{
	Q_OBJECT
private:
	int number = -1;
public:
	explicit MyToolButton(QWidget *parent = 0);

	void setNumber(int n);
signals:

public slots:

protected:
	void paintEvent(QPaintEvent *event);
};

#endif // MYTOOLBUTTON_H
