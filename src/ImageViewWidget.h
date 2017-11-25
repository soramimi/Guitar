#ifndef IMAGEVIEWWIDGET_H
#define IMAGEVIEWWIDGET_H

#include <QScrollBar>
#include <QWidget>
#include "Git.h"
#include "MainWindow.h"
#include "FileDiffWidget.h"

class FileDiffSliderWidget;

class ImageViewWidget : public QWidget {
	Q_OBJECT
private:
	struct Private;
	Private *m;

	bool isValidImage() const;
	QSize imageSize() const;

	QSizeF imageScrollRange() const;
	void scrollImage(double x, double y);
	void setImageScale(double scale);
//	const TextDiffLineList *getLines() const;
//	ObjectContentPtr getContent() const;
	QBrush getTransparentBackgroundBrush();
	bool hasFocus() const;
	void setScrollBarRange(QScrollBar *h, QScrollBar *v);
	void updateScrollBarRange();
protected:
	void resizeEvent(QResizeEvent *);
	void paintEvent(QPaintEvent *);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *);
	void contextMenuEvent(QContextMenuEvent *);
public:
	explicit ImageViewWidget(QWidget *parent = 0);
	~ImageViewWidget();

	void bind(MainWindow *m, FileDiffWidget *filediffwidget, QScrollBar *vsb, QScrollBar *hsb);

	void clear();

	void setImage(QString mimetype, const QByteArray &ba, const QString &object_id, const QString &path);

	void setLeftBorderVisible(bool f);

	static QString formatText(const Document::Line &line2);
signals:
	void scrollByWheel(int lines);
};

#endif // IMAGEVIEWWIDGET_H
