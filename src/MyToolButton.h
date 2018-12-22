#ifndef MYTOOLBUTTON_H
#define MYTOOLBUTTON_H

#include <QToolButton>

class MyToolButton : public QToolButton {
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
	explicit MyToolButton(QWidget *parent = nullptr);
	void setNumber(int n);
	void setDot(bool f);
protected:
	void paintEvent(QPaintEvent *event) override;
};

#endif // MYTOOLBUTTON_H
