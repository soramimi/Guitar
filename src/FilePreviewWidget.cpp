#include "FilePreviewWidget.h"
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

struct FilePreviewWidget::Private {
	MainWindow *mainwindow = nullptr;
	FileDiffWidget::DiffData *diff_data = nullptr;
	FileDiffWidget::DrawData *draw_data = nullptr;
	QScrollBar *vertical_scroll_bar = nullptr;
	ViewType view_type = ViewType::None;
	QString mime_type;
	QPixmap pixmap;
};

FileDiffWidget::DiffData *FilePreviewWidget::diffdata()
{
	return pv->diff_data;
}

FileDiffWidget::DiffData const *FilePreviewWidget::diffdata() const
{
	return pv->diff_data;
}

FileDiffWidget::DrawData *FilePreviewWidget::drawdata()
{
	return pv->draw_data;
}

FileDiffWidget::DrawData const *FilePreviewWidget::drawdata() const
{
	return pv->draw_data;
}

FilePreviewWidget::FilePreviewWidget(QWidget *parent)
	: QWidget(parent)
{
	pv = new Private();

#if defined(Q_OS_WIN32)
	setFont(QFont("MS Gothic"));
#elif defined(Q_OS_LINUX)
	setFont(QFont("Monospace"));
#elif defined(Q_OS_MAC)
	setFont(QFont("Menlo"));
#endif
}

FilePreviewWidget::~FilePreviewWidget()
{
	delete pv;
}

void FilePreviewWidget::imbue_(MainWindow *m, FileDiffWidget::DiffData *diffdata, FileDiffWidget::DrawData *drawdata)
{
	pv->mainwindow = m;
	pv->diff_data = diffdata;
	pv->draw_data = drawdata;
}

void FilePreviewWidget::update()
{
	QWidget::update();
}

void FilePreviewWidget::update(ViewType vt)
{
	pv->view_type = vt;
	update();
}

void FilePreviewWidget::clear(ViewType vt)
{
	pv->mime_type = QString();
	pv->pixmap = QPixmap();
	update(vt);
}

void FilePreviewWidget::updateDrawData_(int top_margin, int bottom_margin)
{
	QPixmap pm(1, 1);
	QPainter pr(&pm);
	QFontMetrics fm = pr.fontMetrics();
	QSize sz = fm.size(0, "W");
	drawdata()->char_width = sz.width();
	drawdata()->line_height = sz.height() + top_margin + bottom_margin;
}

void FilePreviewWidget::paintText()
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
	if (pv->view_type == ViewType::Left) {
		lines = &diffdata()->left_lines;
	} else if (pv->view_type == ViewType::Right) {
		lines = &diffdata()->right_lines;
	}

	const int linenums = 5;

	if (lines) {
		int char_width = drawdata()->char_width;
		int linenum_w = char_width * linenums;
		int x2 = x + linenum_w;
		int w2 = w - linenum_w;
		if (w2 < 1) return;
		pr.save();
		pr.setClipRect(x, 0, w, h);
		int y = drawdata()->v_scroll_pos * drawdata()->line_height;
		int i = y / drawdata()->line_height;
		y -= i * drawdata()->line_height;
		y = -y;
		while (i < lines->size() && y < h) {
			QString const &line = lines->at(i).line;
			TextDiffLine::Type type = lines->at(i).type;

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

			{ // draw line number
				pr.fillRect(x, y, linenum_w + char_width, drawdata()->line_height, drawdata()->bgcolor_gray);
				int num = lines->at(i).line_number;
				if (num >= 0 && num < lines->size()) {
					QString text = "     " + QString::number(num + 1);
					text = text.mid(text.size() - 5);
					pr.setPen(QColor(128, 128, 128));
					pr.drawText(x, line_y, text);
				}

			}

			// draw text
			int x3 = char_width * (linenums + 1); // 行番号＋ヘッダマーク
			int x4 = drawdata()->h_scroll_pos * char_width; // 水平スクロール
			pr.fillRect(x3 - 1, y, w2 - char_width + 1, drawdata()->line_height, QBrush(*bgcolor));
			pr.setPen(QColor(0, 0, 0));

			switch (type) {
			case TextDiffLine::Add: pr.drawText(x2, line_y, "+"); break;
			case TextDiffLine::Del: pr.drawText(x2, line_y, "-"); break;
			}

			pr.save();
			pr.setClipRect(x3, 0, width() - x3, height());
			pr.drawText(x3 - x4, line_y, line);
			pr.restore();

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

	if (drawdata()->forcus != ViewType::None && pv->view_type == drawdata()->forcus) {
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

void FilePreviewWidget::paintImage()
{
	int w = pv->pixmap.width();
	int h = pv->pixmap.height();
	if (w > 0 && h > 0) {
		QPainter pr(this);
		pr.drawPixmap(0, 0, w, h, pv->pixmap);
	}
}

bool isImageFile(QString const &mimetype)
{
	if (mimetype == "image/jpeg") return true;
	if (mimetype == "image/png") return true;
	if (mimetype == "image/bmp") return true;
	if (mimetype == "image/x-ms-bmp") return true;
	return false;
}

void FilePreviewWidget::paintEvent(QPaintEvent *)
{
	if (isImageFile(pv->mime_type)) {
		paintImage();
	} else {
		paintText();
	}
	if (hasFocus()) {
		QPainter pr(this);
		misc::drawFrame(&pr, 0, 0, width(), height(), QColor(0, 128, 255, 128));
		misc::drawFrame(&pr, 1, 1, width() - 2, height() - 2, QColor(0, 128, 255, 64));
	}
}

void FilePreviewWidget::wheelEvent(QWheelEvent *e)
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

void FilePreviewWidget::resizeEvent(QResizeEvent *)
{
	emit resized();
}

void FilePreviewWidget::contextMenuEvent(QContextMenuEvent *e)
{
	if (!pv->mainwindow) return; // TODO:

	QString id;
	switch (pv->view_type) {
	case ViewType::Left:  id = diffdata()->left_id;  break;
	case ViewType::Right: id = diffdata()->right_id; break;
	}
	if (id.startsWith(PATH_PREFIX)) {
		// pass
	} else if (Git::isValidID(id)) {
		// pass
	} else {
		return; // invalid id
	}

	QPoint pos;
	if (e->reason() == QContextMenuEvent::Mouse) {
		pos = QCursor::pos() + QPoint(8, -8);
	} else {
		pos = mapToGlobal(QPoint(4, 4));
	}

	drawdata()->forcus = pv->view_type;

	QString path = pv->mainwindow->currentWorkingCopyDir() / diffdata()->path;

	QMenu menu;
	QAction *a_save_as = id.isEmpty() ? nullptr : menu.addAction(tr("Save as..."));
	QAction *a_test = id.isEmpty() ? nullptr : menu.addAction(tr("test"));
	if (!menu.actions().isEmpty()) {
		drawdata()->forcus = pv->view_type;
		update(pv->view_type);
		QAction *a = menu.exec(pos);
		if (a) {
			if (a == a_save_as) {
				QString dstpath = QFileDialog::getSaveFileName(window(), tr("Save as"), path);
				if (!dstpath.isEmpty()) {
					pv->mainwindow->saveAs(id, dstpath);
				}
				goto DONE;
			}
			if (a == a_test) {
				QString path = pv->mainwindow->saveAsTemp(id);

				QString mimetype = pv->mainwindow->filetype(path, true);
				if (isImageFile(mimetype)) {
					pv->mime_type = mimetype;
					pv->pixmap.load(path);
				}

				QFile::remove(path);

				goto DONE;
			}
		}
	}
DONE:;
	drawdata()->forcus = ViewType::None;
	update(pv->view_type);
}

