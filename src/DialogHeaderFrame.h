#ifndef DIALOGHEADERFRAME_H
#define DIALOGHEADERFRAME_H

#include <QFrame>

class DialogHeaderFrame : public QFrame {
	Q_OBJECT
public:
	explicit DialogHeaderFrame(QWidget *parent = nullptr);
protected:
	void paintEvent(QPaintEvent *) override;
};

#endif // DIALOGHEADERFRAME_H
