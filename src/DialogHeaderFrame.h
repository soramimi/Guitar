#ifndef DIALOGHEADERFRAME_H
#define DIALOGHEADERFRAME_H

#include <QFrame>

class DialogHeaderFrame : public QFrame
{
	Q_OBJECT
public:
	explicit DialogHeaderFrame(QWidget *parent = nullptr);

signals:

public slots:

	// QWidget interface
protected:
	void paintEvent(QPaintEvent *event);
};

#endif // DIALOGHEADERFRAME_H
