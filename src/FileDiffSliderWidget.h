#ifndef FILEDIFFSLIDERWIDGET_H
#define FILEDIFFSLIDERWIDGET_H

#include "AbstractCharacterBasedApplication.h"
#include "MainWindow.h"
#include "Theme.h"
#include <QPixmap>
#include <QWidget>
#include <functional>

using TextDiffLine = Document::Line;
using TextDiffLineList = QList<Document::Line>;

enum class DiffPane {
	Left,
	Right,
};

using fn_pixmap_maker_t = std::function<QPixmap (DiffPane, int, int)>;

class FileDiffSliderWidget : public QWidget {
	Q_OBJECT
private:
	struct Private;
	Private *m;

	void scroll_(int pos);
	QPixmap makeDiffPixmap(DiffPane pane, int width, int height);
	void setValue(int v);
	void internalSetValue(int v);
public:
	explicit FileDiffSliderWidget(QWidget *parent = nullptr);
	~FileDiffSliderWidget() override;

	void clear(bool v);
	void setScrollPos(int total, int value, int size);
	void init(fn_pixmap_maker_t const &pixmap_maker, const ThemePtr &theme);
	void updatePixmap();
	static QPixmap makeDiffPixmap(int width, int height, TextDiffLineList const &lines, ThemePtr theme);
protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *) override;
	void mousePressEvent(QMouseEvent *) override;
	void mouseMoveEvent(QMouseEvent *) override;
	void wheelEvent(QWheelEvent *event) override;
signals:
	void valueChanged(int value);
	void scrollByWheel(int lines);
};

#endif // FILEDIFFSLIDERWIDGET_H
