#include "ImageViewWidget.h"
#include "FileDiffSliderWidget.h"
#include "FileDiffWidget.h"

#include "MainWindow.h"
#include "common/misc.h"
#include "common/joinpath.h"
#include "Photoshop.h"
#include "MemoryReader.h"
#include "charvec.h"

#include <math.h>
#include <functional>

#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QPainter>
#include <QWheelEvent>
#include <QSvgRenderer>
#include <QBuffer>

typedef std::shared_ptr<QSvgRenderer> SvgRendererPtr;

struct ImageViewWidget::Private {
	MainWindow *mainwindow = nullptr;
	FileDiffWidget *filediffwidget = nullptr;
	FileDiffWidget::DrawData *draw_data = nullptr;
	QScrollBar *v_scroll_bar = nullptr;
	QScrollBar *h_scroll_bar = nullptr;
	QString mime_type;

	QPixmap pixmap;
	SvgRendererPtr svg;

	double image_scroll_x = 0;
	double image_scroll_y = 0;
	double image_scale = 1;
	int scroll_origin_x = 0;
	int scroll_origin_y = 0;
	QPoint mouse_press_pos;
	int wheel_delta = 0;
	QPointF interest_pos;
	int top_margin = 1;
	int bottom_margin = 1;
	bool draw_left_border = true;

#ifndef APP_GUITAR
	QPixmap transparent_pixmap;
#endif
};

ImageViewWidget::ImageViewWidget(QWidget *parent)
	: QWidget(parent)
	, m(new Private)
{
#if defined(Q_OS_WIN32)
	setFont(QFont("MS Gothic"));
#elif defined(Q_OS_LINUX)
	setFont(QFont("Monospace"));
#elif defined(Q_OS_MAC)
	setFont(QFont("Menlo"));
#endif

	setContextMenuPolicy(Qt::DefaultContextMenu);
}

ImageViewWidget::~ImageViewWidget()
{
	delete m;
}

void ImageViewWidget::bind(MainWindow *mainwindow, FileDiffWidget *filediffwidget, QScrollBar *vsb, QScrollBar *hsb)
{
	m->mainwindow = mainwindow;
	m->filediffwidget = filediffwidget;
	m->v_scroll_bar = vsb;
	m->h_scroll_bar = hsb;
}

bool ImageViewWidget::hasFocus() const
{
	QWidget *w = qApp->focusWidget();
	return w && w != m->filediffwidget && w->isAncestorOf(this);
}

void ImageViewWidget::setLeftBorderVisible(bool f)
{
	m->draw_left_border = f;
}

void ImageViewWidget::internalScrollImage(double x, double y)
{
	m->image_scroll_x = x;
	m->image_scroll_y = y;
	QSizeF sz = imageScrollRange();
	if (m->image_scroll_x < 0) m->image_scroll_x = 0;
	if (m->image_scroll_y < 0) m->image_scroll_y = 0;
	if (m->image_scroll_x > sz.width()) m->image_scroll_x = sz.width();
	if (m->image_scroll_y > sz.height()) m->image_scroll_y = sz.height();
	update();
}

void ImageViewWidget::scrollImage(double x, double y)
{
	internalScrollImage(x, y);

	if (m->h_scroll_bar) {
		m->h_scroll_bar->blockSignals(true);
		m->h_scroll_bar->setValue(m->image_scroll_x);
		m->h_scroll_bar->blockSignals(false);
	}
	if (m->v_scroll_bar) {
		m->v_scroll_bar->blockSignals(true);
		m->v_scroll_bar->setValue(m->image_scroll_y);
		m->v_scroll_bar->blockSignals(false);
	}
}

void ImageViewWidget::refrectScrollBar()
{
	double e = 0.75;
	double x = m->h_scroll_bar->value();
	double y = m->v_scroll_bar->value();
	if (fabs(x - m->image_scroll_x) < e) x = m->image_scroll_x; // 差が小さいときは値を維持する
	if (fabs(y - m->image_scroll_y) < e) y = m->image_scroll_y;
	internalScrollImage(x, y);
}

void ImageViewWidget::clear()
{
	m->mime_type = QString();
	m->pixmap = QPixmap();
	setMouseTracking(false);
	update();
}

QString ImageViewWidget::formatText(Document::Line const &line)
{
	QByteArray const &ba = line.text;
	if (ba.isEmpty()) return QString();
	std::vector<char> vec;
	vec.reserve(ba.size() + 100);
	char const *begin = ba.data();
	char const *end = begin + ba.size();
	char const *ptr = begin;
	int x = 0;
	while (ptr < end) {
		if (*ptr == '\t') {
			do {
				vec.push_back(' ');
				x++;
			} while ((x % 4) != 0);
			ptr++;
		} else {
			vec.push_back(*ptr);
			ptr++;
			x++;
		}
	}
	return QString::fromUtf8(&vec[0], vec.size());
}

QSizeF ImageViewWidget::imageScrollRange() const
{
	QSize sz = imageSize();
	int w = sz.width() * m->image_scale;
	int h = sz.height() * m->image_scale;
	return QSize(w, h);
}

void ImageViewWidget::setScrollBarRange(QScrollBar *h, QScrollBar *v)
{
	h->blockSignals(true);
	v->blockSignals(true);
	QSizeF sz = imageScrollRange();
	h->setRange(0, sz.width());
	v->setRange(0, sz.height());
	h->setPageStep(width());
	v->setPageStep(height());
	h->blockSignals(false);
	v->blockSignals(false);
}

void ImageViewWidget::updateScrollBarRange()
{
	setScrollBarRange(m->h_scroll_bar, m->v_scroll_bar);
}

MainWindow *ImageViewWidget::mainwindow()
{
	return m->mainwindow;
}

QBrush ImageViewWidget::getTransparentBackgroundBrush()
{
#ifdef APP_GUITAR
	return mainwindow()->getTransparentPixmap();
#else
	if (m->transparent_pixmap.isNull()) {
		m->transparent_pixmap = QPixmap(":/image/transparent.png");
	}
	return m->transparent_pixmap;
#endif
}

bool ImageViewWidget::isValidImage() const
{
	return !m->pixmap.isNull() || (m->svg && m->svg->isValid());
}

QSize ImageViewWidget::imageSize() const
{
	if (!m->pixmap.isNull()) return m->pixmap.size();
	if (m->svg && m->svg->isValid()) return m->svg->defaultSize();
	return QSize();
}

void ImageViewWidget::paintEvent(QPaintEvent *)
{
	QPainter pr(this);

	QSize imagesize = imageSize();
	if (imagesize.width() > 0 && imagesize.height() > 0) {
		pr.save();
		if (!m->draw_left_border) {
			pr.setClipRect(1, 0, width() - 1, height());
		}
		double cx = width() / 2.0;
		double cy = height() / 2.0;
		double x = cx - m->image_scroll_x;
		double y = cy - m->image_scroll_y;
		QSizeF sz = imageScrollRange();
		if (sz.width() > 0 && sz.height() > 0) {
			QBrush br = getTransparentBackgroundBrush();
			pr.setBrushOrigin(x, y);
			pr.fillRect(x, y, sz.width(), sz.height(), br);
			if (!m->pixmap.isNull()) {
				pr.drawPixmap(x, y, sz.width(), sz.height(), m->pixmap, 0, 0, imagesize.width(), imagesize.height());
			} else if (m->svg && m->svg->isValid()) {
				m->svg->render(&pr, QRectF(x, y, sz.width(), sz.height()));
			}
		}
		misc::drawFrame(&pr, x - 1, y - 1, sz.width() + 2, sz.height() + 2, Qt::black);
		pr.restore();
	}

	if (m->draw_left_border) {
		pr.fillRect(0, 0, 1, height(), QColor(160, 160, 160));
	}

	if (hasFocus()) {
		misc::drawFrame(&pr, 0, 0, width(), height(), QColor(0, 128, 255, 128));
		misc::drawFrame(&pr, 1, 1, width() - 2, height() - 2, QColor(0, 128, 255, 64));
	}
}

void ImageViewWidget::resizeEvent(QResizeEvent *)
{
	updateScrollBarRange();
}

void ImageViewWidget::setImage(QString mimetype, QByteArray const &ba)
{
	if (mimetype.isEmpty()) {
		mimetype = "image/x-unknown";
	}

	setMouseTracking(true);

	m->pixmap = QPixmap();
	m->svg = SvgRendererPtr();
	if (!ba.isEmpty()) {
		if (misc::isSVG(mimetype)) {
			m->svg = SvgRendererPtr(new QSvgRenderer(ba));
		} else if (misc::isPSD(mimetype)) {
			if (!ba.isEmpty()) {
				MemoryReader reader(ba.data(), ba.size());
				if (reader.open(QIODevice::ReadOnly)) {
					std::vector<char> jpeg;
					photoshop::readThumbnail(&reader, &jpeg);
					if (!jpeg.empty()) {
						m->pixmap.loadFromData((uchar const *)&jpeg[0], jpeg.size());
					}
				}
			}
		} else {
			m->pixmap.loadFromData(ba);
		}
	}
	QSize sz = imageSize();
	double sx = sz.width();
	double sy = sz.height();
	if (sx > 0 && sy > 0) {
		sx = width() / sx;
		sy = height() / sy;
		m->image_scale = (sx < sy ? sx : sy) * 0.9;
	}
	updateScrollBarRange();

	m->h_scroll_bar->blockSignals(true);
	m->v_scroll_bar->blockSignals(true);
	m->h_scroll_bar->setValue(m->h_scroll_bar->maximum() / 2);
	m->v_scroll_bar->setValue(m->v_scroll_bar->maximum() / 2);
	m->h_scroll_bar->blockSignals(false);
	m->v_scroll_bar->blockSignals(false);
	refrectScrollBar();
}

void ImageViewWidget::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton) {
		QPoint pos = mapFromGlobal(QCursor::pos());
		m->mouse_press_pos = pos;
		m->scroll_origin_x = m->image_scroll_x;
		m->scroll_origin_y = m->image_scroll_y;
	}
}

void ImageViewWidget::mouseMoveEvent(QMouseEvent *e)
{
	if (isValidImage()) {
		QPoint pos = mapFromGlobal(QCursor::pos());
		if ((e->buttons() & Qt::LeftButton) && hasFocus()) {
			int delta_x = pos.x() - m->mouse_press_pos.x();
			int delta_y = pos.y() - m->mouse_press_pos.y();
			scrollImage(m->scroll_origin_x - delta_x, m->scroll_origin_y - delta_y);
		}
		double cx = width() / 2.0;
		double cy = height() / 2.0;
		double x = (pos.x() + 0.5 - cx + m->image_scroll_x) / m->image_scale;
		double y = (pos.y() + 0.5 - cy + m->image_scroll_y) / m->image_scale;
		m->interest_pos = QPointF(x, y);
		m->wheel_delta = 0;
	}
}

void ImageViewWidget::setImageScale(double scale)
{
	if (scale < 1 / 32.0) scale = 1 / 32.0;
	if (scale > 32) scale = 32;
	m->image_scale = scale;
}

void ImageViewWidget::wheelEvent(QWheelEvent *e)
{
	if (isValidImage()) {
		double scale = 1;
		const double mul = 1.189207115; // sqrt(sqrt(2))
		m->wheel_delta += e->delta();
		while (m->wheel_delta >= 120) {
			m->wheel_delta -= 120;
			scale *= mul;
		}
		while (m->wheel_delta <= -120) {
			m->wheel_delta += 120;
			scale /= mul;
		}
		setImageScale(m->image_scale * scale);
		updateScrollBarRange();

		double cx = width() / 2.0;
		double cy = height() / 2.0;
		QPoint pos = mapFromGlobal(QCursor::pos());
		double dx = m->interest_pos.x() * m->image_scale + cx - (pos.x() + 0.5);
		double dy = m->interest_pos.y() * m->image_scale + cy - (pos.y() + 0.5);
		scrollImage(dx, dy);

		update();
	}
}



