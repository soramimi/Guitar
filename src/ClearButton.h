#ifndef CLEARBUTTON_H
#define CLEARBUTTON_H

#include <QToolButton>

class ClearButton : public QToolButton
{
	Q_OBJECT
private:
	QPixmap pixmap;
public:
	explicit ClearButton(QWidget *parent = 0);

signals:

public slots:

protected:
	void paintEvent(QPaintEvent *event);
};

#endif // CLEARBUTTON_H
