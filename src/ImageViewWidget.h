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
	static QMainWindow *mainwindow();
	void resizeEvent(QResizeEvent *) override;
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *) override;
public:
	explicit ImageViewWidget(QWidget *parent = nullptr);
	~ImageViewWidget() override;

	void bind(FileDiffWidget *filediffwidget, QScrollBar *vsb, QScrollBar *hsb);

	void clear();

	void setImage(QString mimetype, QByteArray const &ba);

	void setLeftBorderVisible(bool f);

	void refrectScrollBar();

	static QString formatText(const Document::Line &line2);
signals:
	void scrollByWheel(int lines);
};

#endif // IMAGEVIEWWIDGET_H
