#include "FileDiffSliderWidget.h"
#include "MainWindow.h"
#include "misc.h"
#include <QPainter>
#include <QMouseEvent>

struct FileDiffSliderWidget::Private {
	MainWindow *mainwindow;
	FileDiffWidget *file_diff_widget;
	FileDiffWidget::DiffData const *diff_data;
	FileDiffWidget::DrawData const *draw_data;
	bool visible = false;
	int scroll_total = 0;
	int scroll_value = 0;
	int scroll_visible_size = 0;
	QPixmap left_pixmap;
	QPixmap right_pixmap;
};

FileDiffSliderWidget::FileDiffSliderWidget(QWidget *parent)
	: QWidget(parent)
{
	pv = new Private();
}

FileDiffSliderWidget::~FileDiffSliderWidget()
{
	delete pv;
}

void FileDiffSliderWidget::imbue_(MainWindow *mw, FileDiffWidget *fdw, const FileDiffWidget::DiffData *diffdata, const FileDiffWidget::DrawData *drawdata)
{
	pv->mainwindow = mw;
	pv->file_diff_widget = fdw;
	pv->diff_data = diffdata;
	pv->draw_data = drawdata;
}

void FileDiffSliderWidget::updatePixmap()
{
	Q_ASSERT(pv->file_diff_widget);
	pv->left_pixmap = pv->file_diff_widget->makeDiffPixmap(ViewType::Left, 8, height(), pv->diff_data, pv->draw_data);
	pv->right_pixmap = pv->file_diff_widget->makeDiffPixmap(ViewType::Right, 8, height(), pv->diff_data, pv->draw_data);
}

void FileDiffSliderWidget::paintEvent(QPaintEvent *)
{
	if (!pv->visible) return;
	if (pv->scroll_total < 1) return;

	if (pv->left_pixmap.isNull() || pv->right_pixmap.isNull()) {
		updatePixmap();
	}

	int w;
	w = pv->left_pixmap.width();
	QPainter pr(this);
	pr.fillRect(w, 0, 4, height(), QColor(240, 240, 240));
	pr.drawPixmap(0, 0, pv->left_pixmap);
	pr.drawPixmap(w + 4, 0, pv->right_pixmap);

	int y = pv->scroll_value * height() / pv->scroll_total;
	int h = pv->scroll_visible_size * height() / pv->scroll_total;
	if (h < 2) h = 2;
	pr.fillRect(w + 1, y, 2, h, Qt::black);

	if (hasFocus()) {
		QPainter pr(this);
		misc::drawFrame(&pr, 0, 0, width(), height(), QColor(0, 128, 255, 128));
	}
}

void FileDiffSliderWidget::resizeEvent(QResizeEvent *)
{
	clear(pv->visible);
}

void FileDiffSliderWidget::scroll(int pos)
{
	int v = pos;
	v = v * pv->scroll_total / height() - pv->scroll_visible_size / 2;
	if (v < 0) v = 0;

	emit valueChanged(v);
}

void FileDiffSliderWidget::mousePressEvent(QMouseEvent *e)
{
	if (pv->visible && e->button() == Qt::LeftButton) {
		scroll(e->pos().y());
	}
}

void FileDiffSliderWidget::mouseMoveEvent(QMouseEvent *e)
{
	if (pv->visible && (e->buttons() & Qt::LeftButton)) {
		scroll(e->pos().y());
	}
}

void FileDiffSliderWidget::wheelEvent(QWheelEvent *e)
{
	int delta = e->delta();
	if (delta < 0) {
		delta = -delta / 40;
		if (delta == 0) delta = 1;
		emit scrollByWheel(delta);
	} else if (delta > 0) {
		delta /= 40;
		if (delta == 0) delta = 1;
		emit scrollByWheel(-delta);

	}
}

void FileDiffSliderWidget::clear(bool v)
{
	pv->left_pixmap = QPixmap();
	pv->right_pixmap = QPixmap();
	pv->visible = v;
	update();
}

void FileDiffSliderWidget::setScrollPos(int total, int value, int size)
{
	pv->scroll_total = total;
	pv->scroll_value = value;
	pv->scroll_visible_size = size;
	pv->visible = (pv->scroll_total > 0) && (pv->scroll_visible_size > 0);
	update();
}
