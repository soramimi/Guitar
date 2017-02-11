#ifndef FILEPREVIEWWIDGET_H
#define FILEPREVIEWWIDGET_H

#include <QScrollBar>
#include <QWidget>
#include "Git.h"
#include "MainWindow.h"
#include "FileDiffWidget.h"

class FileDiffSliderWidget;

class FilePreviewWidget : public QWidget
{
	Q_OBJECT
private:
	struct Private;
	Private *pv;

	enum class PaintMode {
		None,
		Text,
		Image,
	};

	FileDiffWidget::DrawData *drawdata();
	const FileDiffWidget::DrawData *drawdata() const;
	void paintText();
	void paintImage();
	QSizeF imageScrollRange() const;
	void scrollImage(double x, double y);
	void setFileType(QString const &mimetype);
	void setImageScale(double scale);
	const QList<TextDiffLine> *getLines() const;
	const FileDiffWidget::DiffData::Content *getContent() const;
	void updateDrawData(QPainter *painter, int *descent = nullptr);
	void updateDrawData();
public:
	explicit FilePreviewWidget(QWidget *parent);
	~FilePreviewWidget();

	void bind(MainWindow *m, const FileDiffWidget::DiffData::Content *content, FileDiffWidget::DrawData *drawdata);

	void clear();

	void setImage(QString mimetype, QPixmap pixmap);
protected:
	void paintEvent(QPaintEvent *);
	void wheelEvent(QWheelEvent *);
	void resizeEvent(QResizeEvent *);
	void contextMenuEvent(QContextMenuEvent *);
signals:
	void scrollByWheel(int lines);
	void resized();

	// QWidget interface
protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
};

#endif // FILEPREVIEWWIDGET_H
