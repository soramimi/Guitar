#ifndef FILEDIFFSLIDERWIDGET_H
#define FILEDIFFSLIDERWIDGET_H

#include <QWidget>
#include <QPixmap>

#include "FileDiffWidget.h"
#include "FilePreviewWidget.h"

class FileDiffSliderWidget : public QWidget
{
	Q_OBJECT
private:
	struct Private;
	Private *m;

	void scroll(int pos);
	void updatePixmap();
public:
	explicit FileDiffSliderWidget(QWidget *parent = 0);
	~FileDiffSliderWidget();

	void bind(MainWindow *mw, FileDiffWidget *fdw, const FileDiffWidget::DiffData *diffdata, const FileDiffWidget::DrawData *drawdata);

	void clear(bool v);
	void setScrollPos(int total, int value, int size);
protected:
	void paintEvent(QPaintEvent *);
	void resizeEvent(QResizeEvent *);
	void mousePressEvent(QMouseEvent *);
	void mouseMoveEvent(QMouseEvent *);
	void wheelEvent(QWheelEvent *event);
signals:
	void valueChanged(int value);
	void scrollByWheel(int lines);
public slots:
};

#endif // FILEDIFFSLIDERWIDGET_H
