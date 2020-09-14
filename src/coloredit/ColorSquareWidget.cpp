#include "ColorSquareWidget.h"
#include "MainWindow.h"
#include <QDebug>
#include <QFile>
#include <QPainter>
#include <stdint.h>
#include <QMouseEvent>
//#include <omp.h>

struct ColorSquareWidget::Private {
	int hue = 0;
	int sat = 255;
	int val = 255;
	QImage image;
	QRect rect;
};

ColorSquareWidget::ColorSquareWidget(QWidget *parent)
	: QWidget(parent)
	, m(new Private)
{
	QString path = ":/cl/example.cl";
	std::string source;
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		QByteArray ba = file.readAll();
		if (!ba.isEmpty()) {
			source = std::string(ba.begin(), ba.end());
		}
	}

#if USE_OPENCL
	prog.build(getCL(), source, "saturation_brightness");
#endif
}

ColorSquareWidget::~ColorSquareWidget()
{
	delete m;
}

static void drawFrame(QPainter *pr, int x, int y, int w, int h)
{
	if (w < 3 || h < 3) {
		pr->fillRect(x, y, w, h, Qt::black);
	} else {
		pr->fillRect(x, y, w - 1, 1, Qt::black);
		pr->fillRect(x, y + 1, 1, h - 1, Qt::black);
		pr->fillRect(x + w - 1, y, 1, h - 1, Qt::black);
		pr->fillRect(x + 1, y + h - 1, w - 1, 1, Qt::black);
	}
}

#if USE_OPENCL
MiraCL *SaturationBrightnessWidget::getCL()
{
	return theApp->getCL();
}
#endif

QImage ColorSquareWidget::createImage(int w, int h)
{
	QImage image(w, h, QImage::Format_RGB32);
#if USE_OPENCL
	QColor color = QColor::fromHsv(hue, 255, 255);
	cl_uint r = color.red();
	cl_uint g = color.green();
	cl_uint b = color.blue();
	MiraCL::Buffer buff(getCL());
	buff.alloc(w * h * sizeof(uint32_t));
	prog.arg(0, &w);
	prog.arg(1, &h);
	prog.arg(2, &r);
	prog.arg(3, &g);
	prog.arg(4, &b);
	prog.arg(5, &buff);
	prog.run(w, h);
	buff.read(0, w * h * sizeof(uint32_t), image.bits());
	getCL()->flush();
	getCL()->finish();
#else
	for (int y = 0; y < h; y++) {
		int bri = 255 - 255 * y / h;
		QRgb *dst = (QRgb *)image.scanLine(y);
		for (int x = 0; x < w; x++) {
			int sat = 256 * x / w;
			QColor color = QColor::fromHsv(m->hue, sat, bri);
			dst[x] = qRgb(color.red(), color.green(), color.blue());
		}
	}
#endif
	return image;
}

MainWindow *ColorSquareWidget::mainwindow()
{
	return qobject_cast<MainWindow *>(window());
}

void ColorSquareWidget::updatePixmap(bool force)
{
	int w = width();
	int h = height();
	if (w > 1 && h > 1) {
		if ((m->image.width() != w || m->image.height() != h) || force) {
			m->image = createImage(w, h);
			m->rect = QRect(0, 0, w, h);
		}
	} else {
		m->image = QImage();
	}
}

void ColorSquareWidget::paintEvent(QPaintEvent *)
{
	updatePixmap(false);
	if (!m->image.isNull()) {
		QPainter pr(this);
		pr.drawImage(0, 0, m->image);
		drawFrame(&pr, 0, 0, width(), height());
	}
}

void ColorSquareWidget::press(QPoint const &pos)
{
	int x = pos.x() - m->rect.x();
	int y = pos.y() - m->rect.y();
	x = std::max(0, std::min(x, m->image.width() - 1));
	y = std::max(0, std::min(y, m->image.height() - 1));
	m->sat = x * 255 / m->image.width();
	m->val = 255 - y * 255 / m->image.height();
	emit changeColor(QColor::fromHsv(m->hue, m->sat, m->val));
}

void ColorSquareWidget::mousePressEvent(QMouseEvent *event)
{
	press(event->pos());
}

void ColorSquareWidget::mouseMoveEvent(QMouseEvent *event)
{
	press(event->pos());
}

void ColorSquareWidget::setHue(int h)
{
	if (h < 0) {
		h = 360 - (-h % 360);
	} else {
		h %= 360;
	}
	m->hue = h;
	updatePixmap(true);
	emit changeColor(QColor::fromHsv(m->hue, m->sat, m->val));
	update();
}



