#include "FileDiffSliderWidget.h"
#include "MainWindow.h"
#include "common/misc.h"
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
	, m(new Private)
{
}

FileDiffSliderWidget::~FileDiffSliderWidget()
{
	delete m;
}

void FileDiffSliderWidget::bind(MainWindow *mw, FileDiffWidget *fdw, const FileDiffWidget::DiffData *diffdata, const FileDiffWidget::DrawData *drawdata)
{
	m->mainwindow = mw;
	m->file_diff_widget = fdw;
	m->diff_data = diffdata;
	m->draw_data = drawdata;
}

void FileDiffSliderWidget::updatePixmap()
{
	Q_ASSERT(m->file_diff_widget);
	m->left_pixmap = m->file_diff_widget->makeDiffPixmap(ViewType::Left, 8, height(), m->diff_data, m->draw_data);
	m->right_pixmap = m->file_diff_widget->makeDiffPixmap(ViewType::Right, 8, height(), m->diff_data, m->draw_data);
}

void FileDiffSliderWidget::paintEvent(QPaintEvent *)
{
	if (!m->visible) return;
	if (m->scroll_total < 1) return;

	if (m->left_pixmap.isNull() || m->right_pixmap.isNull()) {
		updatePixmap();
	}

	int w;
	w = m->left_pixmap.width();
	QPainter pr(this);
	pr.fillRect(w, 0, 4, height(), QColor(240, 240, 240));
	pr.drawPixmap(0, 0, m->left_pixmap);
	pr.drawPixmap(w + 4, 0, m->right_pixmap);

	int y = m->scroll_value * height() / m->scroll_total;
	int h = m->scroll_visible_size * height() / m->scroll_total;
	if (h < 2) h = 2;
	pr.fillRect(w + 1, y, 2, h, Qt::black);

	if (hasFocus()) {
		QPainter pr(this);
		misc::drawFrame(&pr, 0, 0, width(), height(), QColor(0, 128, 255, 128));
	}
}

void FileDiffSliderWidget::resizeEvent(QResizeEvent *)
{
	clear(m->visible);
}

void FileDiffSliderWidget::scroll(int pos)
{
	int v = pos;
	v = v * m->scroll_total / height() - m->scroll_visible_size / 2;
	if (v < 0) v = 0;

	emit valueChanged(v);
}

void FileDiffSliderWidget::mousePressEvent(QMouseEvent *e)
{
	if (m->visible && e->button() == Qt::LeftButton) {
		scroll(e->pos().y());
	}
}

void FileDiffSliderWidget::mouseMoveEvent(QMouseEvent *e)
{
	if (m->visible && (e->buttons() & Qt::LeftButton)) {
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
	m->left_pixmap = QPixmap();
	m->right_pixmap = QPixmap();
	m->visible = v;
	update();
}

void FileDiffSliderWidget::setScrollPos(int total, int value, int size)
{
	m->scroll_total = total;
	m->scroll_value = value;
	m->scroll_visible_size = size;
	m->visible = (m->scroll_total > 0) && (m->scroll_visible_size > 0);
	update();
}
