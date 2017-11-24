#include "FileDiffSliderWidget.h"
#include "common/misc.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDebug>

struct FileDiffSliderWidget::Private {
	FileDiffWidget *owner = nullptr;
//	FileDiffWidget *file_diff_widget;
//	FileDiffWidget::DiffData const *diff_data;
//	FileDiffWidget::DrawData const *draw_data;
	bool visible = false;
	int scroll_total = 0;
	int scroll_value = 0;
	int scroll_page_size = 0;
	QPixmap left_pixmap;
	QPixmap right_pixmap;
	int wheel_delta = 0;
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

void FileDiffSliderWidget::bind(FileDiffWidget *w)
{
	m->owner = w;
}

//void FileDiffSliderWidget::bind(MainWindow *mw, FileDiffWidget *fdw, const FileDiffWidget::DiffData *diffdata, const FileDiffWidget::DrawData *drawdata)
//{
//	m->mainwindow = mw;
//	m->file_diff_widget = fdw;
//	m->diff_data = diffdata;
//	m->draw_data = drawdata;
//}



QPixmap FileDiffSliderWidget::makeDiffPixmap(FileDiffWidget::Pane pane, int width, int height)
{
	Q_ASSERT(m->owner);
	return m->owner->makeDiffPixmap(pane, width, height);
}

void FileDiffSliderWidget::updatePixmap()
{
//	Q_ASSERT(m->file_diff_widget);
	m->left_pixmap = makeDiffPixmap(FileDiffWidget::Pane::Left, 1, height());
	m->right_pixmap = makeDiffPixmap(FileDiffWidget::Pane::Right, 1, height());
}

void FileDiffSliderWidget::paintEvent(QPaintEvent *)
{
	if (!m->visible) return;
	if (m->scroll_total < 1) return;

	if (m->left_pixmap.isNull() || m->right_pixmap.isNull()) {
		updatePixmap();
	}

	QPainter pr(this);
	int w = (width() - 4) / 2;
	{
		int h = height();
		pr.fillRect(w, 0, 4, height(), QColor(240, 240, 240));
		int sw = m->left_pixmap.width();
		int sh = m->left_pixmap.height();
		pr.drawPixmap(0, 0, w, h, m->left_pixmap, 0, 0, sw, sh);
		pr.drawPixmap(w + 4, 0, w, h, m->right_pixmap, 0, 0, sw, sh);
	}

	int y = m->scroll_value * height() / m->scroll_total;
	int h = m->scroll_page_size * height() / m->scroll_total;
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

void FileDiffSliderWidget::setValue(int v)
{
	int max = m->scroll_total - m->scroll_page_size / 2;
	if (v > max) {
		v = max;
	}
	if (v < 0) {
		v = 0;
	}
	m->scroll_value = v;
	update();
	emit valueChanged(m->scroll_value);
}

void FileDiffSliderWidget::scroll_(int pos)
{
	int v = pos;
	v = v * m->scroll_total / height() - m->scroll_page_size / 2;
	setValue(v);
}

void FileDiffSliderWidget::mousePressEvent(QMouseEvent *e)
{
	if (m->visible && e->button() == Qt::LeftButton) {
		scroll_(e->pos().y());
	}
}

void FileDiffSliderWidget::mouseMoveEvent(QMouseEvent *e)
{
	if (m->visible && (e->buttons() & Qt::LeftButton)) {
		scroll_(e->pos().y());
	}
}

void FileDiffSliderWidget::wheelEvent(QWheelEvent *e)
{
	int pos = 0;
	m->wheel_delta += e->delta();
	while (m->wheel_delta >= 40) {
		m->wheel_delta -= 40;
		pos--;
	}
	while (m->wheel_delta <= -40) {
		m->wheel_delta += 40;
		pos++;
	}
	if (pos != 0) {
		setValue(m->scroll_value + pos);
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
	m->scroll_page_size = size;
	m->visible = (m->scroll_total > 0) && (m->scroll_page_size > 0);
	update();
}
