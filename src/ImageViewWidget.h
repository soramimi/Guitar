#ifndef IMAGEVIEWWIDGET_H
#define IMAGEVIEWWIDGET_H

#include <QScrollBar>
#include <QWidget>
#include "Git.h"
#include "MainWindow.h"
#include "FileDiffWidget.h"

class FileDiffSliderWidget;

class ImageViewWidget : public QWidget
{
	Q_OBJECT
public:
private:
	struct Private;
	Private *m;

//	FileDiffWidget::DrawData *drawdata();
//	const FileDiffWidget::DrawData *drawdata() const;
//	void paintText();
	void paintImage();
	bool isValidImage() const;
	QSize imageSize() const;

	QSizeF imageScrollRange() const;
	void scrollImage(double x, double y);
	void setImageScale(double scale);
	const TextDiffLineList *getLines() const;
	ObjectContentPtr getContent() const;
//	void paintBinary();
	QBrush getTransparentBackgroundBrush();
	bool hasFocus() const;
public:
	explicit ImageViewWidget(QWidget *parent = 0);
	~ImageViewWidget();

	void bind(MainWindow *m);

	void clear();

	void setFileType(QString const &mimetype);
	void setImage(QString mimetype, const QByteArray &ba);

	FileViewType filetype() const;

	void setLeftBorderVisible(bool f);

	static QString formatText(const Document::Line &line2);
protected:
	void paintEvent(QPaintEvent *);
	void wheelEvent(QWheelEvent *);
	void resizeEvent(QResizeEvent *);
	void contextMenuEvent(QContextMenuEvent *);
signals:
	void scrollByWheel(int lines);
	void resized();
	void onBinaryMode();

	// QWidget interface
protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
};

#endif // IMAGEVIEWWIDGET_H
