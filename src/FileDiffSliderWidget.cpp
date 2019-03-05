
#include "FileDiffSliderWidget.h"
#include "ApplicationGlobal.h"
#include "TextEditorTheme.h"
#include "common/misc.h"
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>

struct FileDiffSliderWidget::Private {
	bool visible = false;
	int scroll_total = 0;
	int scroll_value = 0;
	int scroll_page_size = 0;
	QPixmap left_pixmap;
	QPixmap right_pixmap;
	int wheel_delta = 0;
	fn_pixmap_maker_t pixmap_maker;
	ThemePtr theme;
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

void FileDiffSliderWidget::init(fn_pixmap_maker_t const &pixmap_maker, ThemePtr const &theme)
{
	m->pixmap_maker = pixmap_maker;
	m->theme = theme;
}

QPixmap FileDiffSliderWidget::makeDiffPixmap(DiffPane pane, int width, int height)
{
	if (m->pixmap_maker) {
		return m->pixmap_maker(pane, width, height);
	}
	return QPixmap();
}

void FileDiffSliderWidget::updatePixmap()
{
	m->left_pixmap = makeDiffPixmap(DiffPane::Left, 1, height());
	m->right_pixmap = makeDiffPixmap(DiffPane::Right, 1, height());
}

QPixmap FileDiffSliderWidget::makeDiffPixmap(int width, int height, TextDiffLineList const &lines, ThemePtr theme)
{
	auto MakePixmap = [&](TextDiffLineList const &lines, int w, int h){
		const int scale = 1;
		QPixmap pixmap = QPixmap(w, h * scale);
		pixmap.fill(theme->diff_slider_normal_bg);
		QPainter pr(&pixmap);
		auto Loop = [&](std::function<QColor(TextDiffLine::Type)> getcolor){
			int i = 0;
			while (i < lines.size()) {
				auto type = (TextDiffLine::Type)lines[i].type;
				int j = i + 1;
				if (type != TextDiffLine::Normal) {
					while (j < lines.size()) {
						if (lines[j].type != type) break;
						j++;
					}
					int y = i * pixmap.height() / lines.size();
					int z = j * pixmap.height() / lines.size();
					if (z == y) z = y + 1;
					QColor color = getcolor(type);
					if (color.isValid()) pr.fillRect(0, y, w, z - y, color);
				}
				i = j;
			}
		};
		Loop([&](TextDiffLine::Type t)->QColor{
			switch (t) {
			case TextDiffLine::Unknown: return theme->diff_slider_unknown_bg;
			}
			return QColor();
		});
		Loop([&](TextDiffLine::Type t)->QColor{
			switch (t) {
			case TextDiffLine::Add: return theme->diff_slider_add_bg;
			case TextDiffLine::Del: return theme->diff_slider_del_bg;
			}
			return QColor();
		});
		if (scale == 1) return pixmap;
		return pixmap.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	};
	return MakePixmap(lines, width, height);
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

	pr.fillRect(w, 0, 4, height(), palette().color(QPalette::Window));
	if (m->scroll_page_size > 0 && m->scroll_total > 0) {
		int y = m->scroll_value * height() / m->scroll_total;
		int h = m->scroll_page_size * height() / m->scroll_total;
		if (h < 2) h = 2;
		pr.fillRect(w + 1, y, 2, h, m->theme->diff_slider_handle);
	}

	if (hasFocus()) {
		QPainter pr(this);
		misc::drawFrame(&pr, 0, 0, width(), height(), QColor(0, 128, 255, 128));
	}
}

void FileDiffSliderWidget::resizeEvent(QResizeEvent *)
{
	clear(m->visible);
}

void FileDiffSliderWidget::internalSetValue(int v)
{
	int max = m->scroll_total - m->scroll_page_size / 2;
	if (v > max) {
		v = max;
	}
	if (v < 0) {
		v = 0;
	}
	m->scroll_value = v;
}

void FileDiffSliderWidget::setValue(int v)
{
	internalSetValue(v);
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
	m->scroll_page_size = size;
	internalSetValue(value);
	m->visible = (m->scroll_total > 0) && (m->scroll_page_size > 0);
	update();
}
