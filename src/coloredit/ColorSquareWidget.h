#ifndef COLORSQUAREWIDGET_H
#define COLORSQUAREWIDGET_H

#include <QWidget>
#include <QPixmap>

class MainWindow;

class ColorSquareWidget : public QWidget {
	Q_OBJECT
private:
#if USE_OPENCL
	MiraCL *getCL();
	MiraCL::Program prog;
#endif
	struct Private;
	Private *m;
	MainWindow *mainwindow();
	void updatePixmap(bool force);
	QImage createImage(int w, int h);
	void press(const QPoint &pos);
protected:
	void paintEvent(QPaintEvent *);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
public:
	explicit ColorSquareWidget(QWidget *parent = 0);
	~ColorSquareWidget();
	void setHue(int h);
signals:
	void changeColor(const QColor &color);

};

#endif // COLORSQUAREWIDGET_H
