#ifndef HYPERLINKLABEL_H
#define HYPERLINKLABEL_H

#include <QLabel>

class HyperLinkLabel : public QLabel {
	Q_OBJECT
public:
	explicit HyperLinkLabel(QWidget *parent = nullptr);
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
signals:
	void clicked();
};

#endif // HYPERLINKLABEL_H
