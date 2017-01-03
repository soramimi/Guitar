#include "FileDiffWidget.h"
#include "FileDiffSliderWidget.h"

#include "MainWindow.h"
#include "misc.h"
#include "joinpath.h"

#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QPainter>
#include <QWheelEvent>
#include <functional>

DiffWidgetData::DiffData *FileDiffWidget::diffdata()
{
	MainWindow *mw = qobject_cast<MainWindow *>(window());
	Q_ASSERT(mw);
	return &mw->getDiffWidgetData()->diffdata;
}

DiffWidgetData::DiffData const *FileDiffWidget::diffdata() const
{
	MainWindow *mw = qobject_cast<MainWindow *>(window());
	Q_ASSERT(mw);
	return &mw->getDiffWidgetData()->diffdata;
}

DiffWidgetData::DrawData *FileDiffWidget::drawdata()
{
	MainWindow *mw = qobject_cast<MainWindow *>(window());
	Q_ASSERT(mw);
	return &mw->getDiffWidgetData()->drawdata;
}

DiffWidgetData::DrawData const *FileDiffWidget::drawdata() const
{
	MainWindow *mw = qobject_cast<MainWindow *>(window());
	Q_ASSERT(mw);
	return &mw->getDiffWidgetData()->drawdata;
}

FileDiffWidget::FileDiffWidget(QWidget *parent)
	: QWidget(parent)
{
#if defined(Q_OS_WIN32)
	setFont(QFont("MS Gothic"));
#elif defined(Q_OS_LINUX)
	setFont(QFont("Monospace"));
#elif defined(Q_OS_MAC)
	setFont(QFont("Menlo"));
#endif
}


FileDiffWidget::~FileDiffWidget()
{
}

void FileDiffWidget::update(ViewType vt)
{
	view_type = vt;
	QWidget::update();
}

void FileDiffWidget::paintEvent(QPaintEvent *)
{
	QPainter pr(this);

	int x;
	int w = width();
	int h = height();
	int top_margin = 1;
	int bottom_margin = 1;

	QFontMetrics fm = pr.fontMetrics();
	int descent = fm.descent();
	QSize sz = fm.size(0, "W");
	drawdata()->char_width = sz.width();
	drawdata()->line_height = sz.height() + top_margin + bottom_margin;

	x = 0;
	QList<TextDiffLine> const *lines = nullptr;
	if (view_type == ViewType::Left) {
		lines = &diffdata()->left_lines;
	} else if (view_type == ViewType::Right) {
		lines = &diffdata()->right_lines;
	}
	if (lines) {
		int linenum_w = sz.width() * 5 + 1;
		int x2 = x + linenum_w;
		int w2 = w - linenum_w;
		if (w2 < 1) return;
		pr.save();
		pr.setClipRect(x, 0, w, h);
		int y = drawdata()->scrollpos * drawdata()->line_height;
		int i = y / drawdata()->line_height;
		y -= i * drawdata()->line_height;
		y = -y;
		while (i < lines->size() && y < h) {
			QString const &line = lines->at(i).line;
			QColor *bgcolor;
			switch (lines->at(i).type) {
			case TextDiffLine::Unchanged:
				bgcolor = &drawdata()->bgcolor_text;
				break;
			case TextDiffLine::Add:
				bgcolor = &drawdata()->bgcolor_add;
				break;
			case TextDiffLine::Del:
				bgcolor = &drawdata()->bgcolor_del;
				break;
			default:
				bgcolor = &drawdata()->bgcolor_gray;
				break;
			}
			int line_y = y + drawdata()->line_height - descent - bottom_margin;
			{
				pr.fillRect(x, y, linenum_w + drawdata()->char_width, drawdata()->line_height, drawdata()->bgcolor_gray);
				int num = lines->at(i).line_number;
				if (num >= 0 && num < lines->size()) {
					QString text = "     " + QString::number(num + 1);
					text = text.mid(text.size() - 5);
					pr.setPen(QColor(128, 128, 128));
					pr.drawText(x, line_y, text);
				}

			}
			pr.fillRect(x2 + drawdata()->char_width, y, w2 - drawdata()->char_width, drawdata()->line_height, QBrush(*bgcolor));
			pr.setPen(QColor(0, 0, 0));
			pr.drawText(x2, line_y, line);
			y += drawdata()->line_height;
			i++;
		}
		if (y < h) {
			pr.fillRect(x, y, w, h - y, QColor(192, 192, 192));
			pr.fillRect(x, y, w, 1, Qt::black);
		}
		pr.fillRect(x, 0, 1, h, QColor(160, 160, 160));
		pr.restore();
	}

	if (drawdata()->forcus != ViewType::None && view_type == drawdata()->forcus) {
		int x = 0;
		int y = 0;
		int w = width();
		int h = height();
		if (w > 0 && h > 0) {
			QColor color(0, 128, 255, 32);
			pr.fillRect(x, y, w, h, color);
		}
	}
}

void FileDiffWidget::wheelEvent(QWheelEvent *e)
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

void FileDiffWidget::resizeEvent(QResizeEvent *)
{
	emit resized();
}

void FileDiffWidget::contextMenuEvent(QContextMenuEvent *)
{
	QPoint pos = QCursor::pos();

	drawdata()->forcus = view_type;

	Git::BLOB blob;
	switch (view_type) {
	case ViewType::Left:  blob = diffdata()->left;  break;
	case ViewType::Right: blob = diffdata()->right; break;
	}

	QMenu menu;
	QAction *a_save_as = blob.id.isEmpty() ? nullptr : menu.addAction(tr("Save as..."));
	if (!menu.actions().isEmpty()) {
		drawdata()->forcus = view_type;
		update(view_type);
		QAction *a = menu.exec(pos + QPoint(8, -8));
		if (a) {
			if (a == a_save_as) {
				if (!blob.id.isEmpty()) {
					blob.path = QFileDialog::getSaveFileName(window(), tr("Save as"), blob.path);
					if (!blob.path.isEmpty()) {
						MainWindow *mw = qobject_cast<MainWindow *>(window());
						Q_ASSERT(mw);
						if (blob.id.startsWith(PATH_PREFIX)) {
							mw->saveFileAs(blob.id.mid(1), blob.path);
						} else {
							mw->saveBlobAs(blob.id, blob.path);
						}
					}
				}
			}
		}
	}
	drawdata()->forcus = ViewType::None;
	update(view_type);
}

