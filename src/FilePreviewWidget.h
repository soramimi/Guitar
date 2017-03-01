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
public:
private:
	struct Private;
	Private *pv;

	FileDiffWidget::DrawData *drawdata();
	const FileDiffWidget::DrawData *drawdata() const;
	void paintText();
	void paintImage();
	QSizeF imageScrollRange() const;
	void scrollImage(double x, double y);
	void setImageScale(double scale);
	const QList<TextDiffLine> *getLines() const;
	ObjectContentPtr getContent() const;
	void updateDrawData(QPainter *painter, int *descent = nullptr);
	void updateDrawData();
	void paintBinary();
public:
	explicit FilePreviewWidget(QWidget *parent);
	~FilePreviewWidget();

	void bind(MainWindow *m, ObjectContentPtr content, FileDiffWidget::DrawData *drawdata);

	void clear();

	void setFileType(QString const &mimetype);
	void setImage(QString mimetype, QPixmap pixmap);

	FilePreviewType filetype() const;

	void setLeftBorderVisible(bool f);
	void setBinaryMode(bool f);
	bool isBinaryMode() const;

	static QString formatText(std::vector<ushort> const &text);
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

#endif // FILEPREVIEWWIDGET_H
