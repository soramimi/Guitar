#ifndef FILEDIFFWIDGET_H
#define FILEDIFFWIDGET_H

#include <QScrollBar>
#include <QWidget>
#include "Git.h"
#include "MainWindow.h"

class FileDiffSliderWidget;

class FileDiffWidget : public QWidget
{
	Q_OBJECT
private:
	DiffWidgetData *diff_widget_data = nullptr;
	QScrollBar *vertical_scroll_bar = nullptr;
	ViewType view_type = ViewType::None;
	QString mime_type;
	QPixmap pixmap;

	DiffWidgetData::DiffData *diffdata();
	const DiffWidgetData::DiffData *diffdata() const;
	DiffWidgetData::DrawData *drawdata();
	const DiffWidgetData::DrawData *drawdata() const;
	void paintText();
	void paintImage();
public:
	explicit FileDiffWidget(QWidget *parent);
	~FileDiffWidget();

	void imbue_(DiffWidgetData *d)
	{
		diff_widget_data = d;
	}

	void update(ViewType vt);

	void clear(ViewType vt);
	void updateDrawData_(int top_margin, int bottom_margin);
protected:
	void paintEvent(QPaintEvent *);
	void wheelEvent(QWheelEvent *);
	void resizeEvent(QResizeEvent *);
	void contextMenuEvent(QContextMenuEvent *);
signals:
	void scrollByWheel(int lines);
	void resized();
};

#endif // FILEDIFFWIDGET_H
