#ifndef MYTOOLBUTTON_H
#define MYTOOLBUTTON_H

#include <QToolButton>

class MyToolButton : public QToolButton
{
	Q_OBJECT
public:
	enum Indicator {
		None,
		Dot,
		Number,
	};
private:
	Indicator indicator = None;
	int number = -1;
	void setIndicatorMode(Indicator i);
public:
	explicit MyToolButton(QWidget *parent = 0);
	void setNumber(int n);
	void setDot(bool f);
signals:

public slots:

protected:
	void paintEvent(QPaintEvent *event);
};

#endif // MYTOOLBUTTON_H
