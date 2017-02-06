#ifndef MAXIMIZEBUTTON_H
#define MAXIMIZEBUTTON_H

#include <QToolButton>

class MaximizeButton : public QToolButton
{
	Q_OBJECT
private:
	QPixmap pixmap;
public:
	explicit MaximizeButton(QWidget *parent = 0);

signals:

public slots:

	// QWidget interface
protected:
	void paintEvent(QPaintEvent *event);
};

#endif // MAXIMIZEBUTTON_H
