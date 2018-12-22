#ifndef CLEARBUTTON_H
#define CLEARBUTTON_H

#include <QToolButton>

class ClearButton : public QToolButton {
	Q_OBJECT
private:
	QPixmap pixmap;
public:
	explicit ClearButton(QWidget *parent = nullptr);
protected:
	void paintEvent(QPaintEvent *event) override;
};

#endif // CLEARBUTTON_H
