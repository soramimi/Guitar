#ifndef FILEDIFFSLIDERWIDGET_H
#define FILEDIFFSLIDERWIDGET_H

#include "MainWindow.h"

#include <QWidget>
#include <QPixmap>
#include <functional>

#include "AbstractCharacterBasedApplication.h"
#include "Theme.h"

typedef Document::Line TextDiffLine;
typedef QList<Document::Line> TextDiffLineList;

enum class DiffPane {
	Left,
	Right,
};

typedef std::function<QPixmap(DiffPane pane, int width, int height)> fn_pixmap_maker_t;

class FileDiffSliderWidget : public QWidget
{
	Q_OBJECT
private:
	struct Private;
	Private *m;

	void scroll_(int pos);
	QPixmap makeDiffPixmap(DiffPane pane, int width, int height);
	void setValue(int v);
	void internalSetValue(int v);
public:
	explicit FileDiffSliderWidget(QWidget *parent = 0);
	~FileDiffSliderWidget();

	void clear(bool v);
	void setScrollPos(int total, int value, int size);
	void init(fn_pixmap_maker_t pixmap_maker, ThemePtr theme);
	void updatePixmap();
	static QPixmap makeDiffPixmap(int width, int height, const TextDiffLineList &lines, ThemePtr theme);
protected:
	void paintEvent(QPaintEvent *);
	void resizeEvent(QResizeEvent *);
	void mousePressEvent(QMouseEvent *);
	void mouseMoveEvent(QMouseEvent *);
	void wheelEvent(QWheelEvent *event);
signals:
	void valueChanged(int value);
	void scrollByWheel(int lines);
};

#endif // FILEDIFFSLIDERWIDGET_H
