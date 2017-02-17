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
	FileDiffWidget::DiffData::Content const *content = nullptr;
	FileDiffWidget::DrawData *draw_data = nullptr;
	QScrollBar *vertical_scroll_bar = nullptr;
	QString mime_type;
	QPixmap pixmap;
	FilePreviewType file_type = FilePreviewType::None;
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
	bool binary_mode = false;
};

FileDiffWidget::DrawData *FilePreviewWidget::drawdata()
{
	Q_ASSERT(pv->draw_data);
	return pv->draw_data;
}

FileDiffWidget::DrawData const *FilePreviewWidget::drawdata() const
{
	Q_ASSERT(pv->draw_data);
	return pv->draw_data;
}

FileDiffWidget::DiffData::Content const *FilePreviewWidget::getContent() const
{
	return pv->content;
}


QList<TextDiffLine> const *FilePreviewWidget::getLines() const
{
	FileDiffWidget::DiffData::Content const *content = getContent();
	if (content) return &content->lines;
	return nullptr;
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

void FilePreviewWidget::bind(MainWindow *m, FileDiffWidget::DiffData::Content const *content, FileDiffWidget::DrawData *drawdata)
{
	pv->mainwindow = m;
	pv->content = content;
	pv->draw_data = drawdata;

	updateDrawData();
}

void FilePreviewWidget::setLeftBorderVisible(bool f)
{
	pv->draw_left_border = f;
}

void FilePreviewWidget::setFileType(QString const &mimetype)
{
	setBinaryMode(false);
	pv->mime_type = mimetype;
	if (misc::isImageFile(pv->mime_type)) {
		pv->file_type = FilePreviewType::Image;
		setMouseTracking(true);
	} else {
		pv->file_type = FilePreviewType::Text;
		setMouseTracking(false);
	}
}

void FilePreviewWidget::clear()
{
	pv->file_type = FilePreviewType::None;
	pv->mime_type = QString();
	pv->pixmap = QPixmap();
	setMouseTracking(false);
	update();
}

void FilePreviewWidget::updateDrawData(QPainter *painter, int *descent)
{
	QFontMetrics fm = painter->fontMetrics();
	QSize sz = fm.size(0, "W");
	drawdata()->char_width = sz.width();
	drawdata()->line_height = sz.height() + pv->top_margin + pv->bottom_margin;
	if (descent) *descent = fm.descent();
}

void FilePreviewWidget::updateDrawData()
{
	QPixmap pm(1, 1);
	QPainter pr(&pm);
	updateDrawData(&pr);
}

void FilePreviewWidget::paintText()
{
	QList<TextDiffLine> const *lines = getLines();
	if (!lines) return;

	QPainter pr(this);

	int x;
	int w = width();
	int h = height();

	int descent;
	updateDrawData(&pr, &descent);

	x = 0;

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
			case TextDiffLine::Normal:
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

			int line_y = y + drawdata()->line_height - descent - pv->bottom_margin;

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
		pr.restore();
	}
}

void FilePreviewWidget::paintBinary()
{
	FileDiffWidget::DiffData::Content const *content = getContent();
	if (!content) return;
	if (content->bytes.isEmpty()) return;

	int length = content->bytes.size();
	uint8_t const *source = (uint8_t const *)content->bytes.data();

	QPainter pr(this);

	int x;
	int w = width();
	int h = height();

	int descent;
	updateDrawData(&pr, &descent);

	x = 0;

	const int linenums = 5;

	{//if (lines) {
		int char_width = drawdata()->char_width;
		int y = drawdata()->v_scroll_pos * drawdata()->line_height;
		y = -y;
		int offset = 0;
		while (offset < length) {
			char tmp[100];
			int j = sprintf(tmp, " %08X ", offset);
			for (int i = 0; i < 16; i++) {
				if (offset + i < length) {
					sprintf(tmp + j, "%02X ", source[offset + i]);
				} else {
					strcpy(tmp + j, "   ");
				}
				j += 3;
			}
			for (int i = 0; i < 16; i++) {
				uint8_t c = '0';
				if (offset < length) {
					c = source[offset];
					if (!isprint(c)) {
						c = '.';
					}
					offset++;
				}
				tmp[j] = c;
				j++;
			}
			tmp[j] = 0;
			int line_y = y + drawdata()->line_height - descent - pv->bottom_margin;
			QColor bgcolor = drawdata()->bgcolor_text;
			pr.fillRect(0, y, width(), drawdata()->line_height, QBrush(bgcolor));
			pr.setPen(QColor(0, 0, 0));
			pr.drawText(x, line_y, tmp);
			y += drawdata()->line_height;
			if (y >= height()) break;
		}
	}
}

QSizeF FilePreviewWidget::imageScrollRange() const
{
	int w = pv->pixmap.width() * pv->image_scale;
	int h = pv->pixmap.height() * pv->image_scale;
	return QSize(w, h);
}

void FilePreviewWidget::paintImage()
{
	QPainter pr(this);
	int w = pv->pixmap.width();
	int h = pv->pixmap.height();
	if (w > 0 && h > 0) {
		pr.save();
		if (!pv->draw_left_border) {
			pr.setClipRect(1, 0, width() - 1, height());
		}
		double cx = width() / 2.0;
		double cy = height() / 2.0;
		double x = cx - pv->image_scroll_x;
		double y = cy - pv->image_scroll_y;
		QSizeF sz = imageScrollRange();
		if (sz.width() > 0 && sz.height() > 0) {
			QBrush br(pv->mainwindow->getTransparentPixmap());
			pr.setBrushOrigin(x, y);
			pr.fillRect(x, y, sz.width(), sz.height(), br);
			pr.drawPixmap(x, y, sz.width(), sz.height(), pv->pixmap, 0, 0, w, h);
		}
		misc::drawFrame(&pr, x - 1, y - 1, sz.width() + 2, sz.height() + 2, Qt::black);
		pr.restore();
	}
}

void FilePreviewWidget::paintEvent(QPaintEvent *)
{
	if (isBinaryMode()) {
		paintBinary();
	} else {
		switch (pv->file_type) {
		case FilePreviewType::Image:
			paintImage();
			break;
		case FilePreviewType::Text:
			paintText();
			break;
		}
	}
	QPainter pr(this);
	if (pv->draw_left_border) {
		pr.fillRect(0, 0, 1, height(), QColor(160, 160, 160));
	}
	if (drawdata()->forcus == this) {
		int x = 0;
		int y = 0;
		int w = width();
		int h = height();
		if (w > 0 && h > 0) {
			QColor color(0, 128, 255, 32);
			pr.fillRect(x, y, w, h, color);
		}
	}
	if (hasFocus()) {
		misc::drawFrame(&pr, 0, 0, width(), height(), QColor(0, 128, 255, 128));
		misc::drawFrame(&pr, 1, 1, width() - 2, height() - 2, QColor(0, 128, 255, 64));
	}
}

void FilePreviewWidget::resizeEvent(QResizeEvent *)
{
	emit resized();
}

void FilePreviewWidget::scrollImage(double x, double y)
{
	pv->image_scroll_x = x;
	pv->image_scroll_y = y;
	QSizeF sz = imageScrollRange();
	if (pv->image_scroll_x < 0) pv->image_scroll_x = 0;
	if (pv->image_scroll_y < 0) pv->image_scroll_y = 0;
	if (pv->image_scroll_x > sz.width()) pv->image_scroll_x = sz.width();
	if (pv->image_scroll_y > sz.height()) pv->image_scroll_y = sz.height();
	update();
}

void FilePreviewWidget::setBinaryMode(bool f)
{
	pv->binary_mode = f;
}

bool FilePreviewWidget::isBinaryMode() const
{
	return pv->binary_mode;
}

void FilePreviewWidget::setImage(QString mimetype, QPixmap pixmap)
{
	setFileType(mimetype);
	if (pv->file_type == FilePreviewType::Image) {
		pv->pixmap = pixmap;
		pv->image_scale = 1;
		double x = pv->pixmap.width() / 2.0;
		double y = pv->pixmap.height() / 2.0;
		scrollImage(x, y);
	}
}

FilePreviewType FilePreviewWidget::filetype() const
{
	return pv->file_type;
}

void FilePreviewWidget::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton) {
		QPoint pos = mapFromGlobal(QCursor::pos());
		pv->mouse_press_pos = pos;
		pv->scroll_origin_x = pv->image_scroll_x;
		pv->scroll_origin_y = pv->image_scroll_y;
	}
}

void FilePreviewWidget::mouseMoveEvent(QMouseEvent *e)
{
	if (pv->file_type == FilePreviewType::Image) {
		if (!pv->pixmap.isNull()) {
			QPoint pos = mapFromGlobal(QCursor::pos());
			if ((e->buttons() & Qt::LeftButton) && focusWidget() == this) {
				int delta_x = pos.x() - pv->mouse_press_pos.x();
				int delta_y = pos.y() - pv->mouse_press_pos.y();
				scrollImage(pv->scroll_origin_x - delta_x, pv->scroll_origin_y - delta_y);
			}
			double cx = width() / 2.0;
			double cy = height() / 2.0;
			double x = (pos.x() + 0.5 - cx + pv->image_scroll_x) / pv->image_scale;
			double y = (pos.y() + 0.5 - cy + pv->image_scroll_y) / pv->image_scale;
			pv->interest_pos = QPointF(x, y);
			pv->wheel_delta = 0;
		}
	}
}

void FilePreviewWidget::setImageScale(double scale)
{
	if (scale < 1 / 32.0) scale = 1 / 32.0;
	if (scale > 32) scale = 32;
	pv->image_scale = scale;
}

void FilePreviewWidget::wheelEvent(QWheelEvent *e)
{
	if (pv->file_type == FilePreviewType::Text) {
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
	} else if (pv->file_type == FilePreviewType::Image) {
		if (!pv->pixmap.isNull()) {
			double scale = 1;
			pv->wheel_delta += e->delta();
			while (pv->wheel_delta >= 120) {
				pv->wheel_delta -= 120;
				scale *= 1.25;
			}
			while (pv->wheel_delta <= -120) {
				pv->wheel_delta += 120;
				scale /= 1.25;
			}
			setImageScale(pv->image_scale * scale);

			double cx = width() / 2.0;
			double cy = height() / 2.0;
			QPoint pos = mapFromGlobal(QCursor::pos());
			double dx = pv->interest_pos.x() * pv->image_scale + cx - (pos.x() + 0.5);
			double dy = pv->interest_pos.y() * pv->image_scale + cy - (pos.y() + 0.5);
			scrollImage(dx, dy);

			update();
		}
	}
}

void FilePreviewWidget::contextMenuEvent(QContextMenuEvent *e)
{
	if (!pv->mainwindow) return; // TODO:

	FileDiffWidget::DiffData::Content const *content = getContent();
	if (!content) return;

	QString id = content->id;
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

	QMenu menu;
	QAction *a_save_as = id.isEmpty() ? nullptr : menu.addAction(tr("Save as..."));
	QAction *a_test = id.isEmpty() ? nullptr : menu.addAction(tr("test"));
	if (!menu.actions().isEmpty()) {
		drawdata()->forcus = this;
		update();
		QAction *a = menu.exec(pos);
		if (a) {
			if (a == a_save_as) {
				QString path = pv->mainwindow->currentWorkingCopyDir() / content->path;
				QString dstpath = QFileDialog::getSaveFileName(window(), tr("Save as"), path);
				if (!dstpath.isEmpty()) {
					pv->mainwindow->saveAs(id, dstpath);
				}
				goto DONE;
			}
			if (a == a_test) {
				emit onBinaryMode();
				goto DONE;
			}
		}
	}
DONE:;
	drawdata()->forcus = nullptr;
	update();
}

