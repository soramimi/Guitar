#ifndef MAXIMIZEBUTTON_H
#define MAXIMIZEBUTTON_H

#include <QToolButton>

class MaximizeButton : public QToolButton {
	Q_OBJECT
private:
	QPixmap pixmap;
public:
	explicit MaximizeButton(QWidget *parent = nullptr);
protected:
	void paintEvent(QPaintEvent *event) override;
};

#endif // MAXIMIZEBUTTON_H
