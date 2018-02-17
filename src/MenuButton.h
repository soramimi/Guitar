#ifndef MENUBUTTON_H
#define MENUBUTTON_H

#include <QToolButton>

class MenuButton : public QToolButton {
	Q_OBJECT
private:
	QPixmap pixmap;
public:
	explicit MenuButton(QWidget *parent = nullptr);
protected:
	void paintEvent(QPaintEvent *event);
};

#endif // MENUBUTTON_H
