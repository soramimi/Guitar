#ifndef IMAGEVIEWWIDGET_H
#define IMAGEVIEWWIDGET_H

#include <QScrollBar>
#include <QWidget>
#include "Git.h"
#include "MainWindow.h"
#include "AbstractCharacterBasedApplication.h"

class FileDiffWidget;

class FileDiffSliderWidget;

class ImageViewWidget : public QWidget {
	Q_OBJECT
private:
	struct Private;
	Private *m;

	bool isValidImage() const;
	QSize imageSize() const;

	QSizeF imageScrollRange() const;
	void internalScrollImage(double x, double y);
	void scrollImage(double x, double y);
	void setImageScale(double scale);
	QBrush getTransparentBackgroundBrush();
	bool hasFocus() const;
	void setScrollBarRange(QScrollBar *h, QScrollBar *v);
	void updateScrollBarRange();
protected:
	MainWindow *mainwindow();
	void resizeEvent(QResizeEvent *);
	void paintEvent(QPaintEvent *);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *);
public:
	explicit ImageViewWidget(QWidget *parent = 0);
	~ImageViewWidget();

	void bind(MainWindow *m, FileDiffWidget *filediffwidget, QScrollBar *vsb, QScrollBar *hsb);

	void clear();

	void setImage(QString mimetype, QByteArray const &ba);

	void setLeftBorderVisible(bool f);

	void refrectScrollBar();

	static QString formatText(const Document::Line &line2);
signals:
	void scrollByWheel(int lines);
};

#endif // IMAGEVIEWWIDGET_H
