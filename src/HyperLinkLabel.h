#ifndef HYPERLINKLABEL_H
#define HYPERLINKLABEL_H

#include <QLabel>

class HyperLinkLabel : public QLabel
{
	Q_OBJECT
public:
	explicit HyperLinkLabel(QWidget *parent = 0);

	void paintEvent(QPaintEvent *);

	void mousePressEvent(QMouseEvent *e);
	void mouseReleaseEvent(QMouseEvent *e);

signals:
	void clicked();

public slots:

};

#endif // HYPERLINKLABEL_H
