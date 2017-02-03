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
		Text,
		Image,
	};

	FileDiffWidget::DiffData *diffdata();
	const FileDiffWidget::DiffData *diffdata() const;
	FileDiffWidget::DrawData *drawdata();
	const FileDiffWidget::DrawData *drawdata() const;
	void paintText();
	void paintImage();
	QSize imageScrollRange() const;
	void scrollImage(int x, int y);
	void setFileType(QString const &mimetype);
public:
	explicit FilePreviewWidget(QWidget *parent);
	~FilePreviewWidget();

	void imbue_(MainWindow *m, FileDiffWidget::DiffData *diffdata, FileDiffWidget::DrawData *drawdata);

	void update();
	void update(ViewType vt);

	void clear(ViewType vt);
	void updateDrawData_(int top_margin, int bottom_margin);
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
