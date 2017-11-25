#include "FileViewWidget.h"
#include "ui_FileViewWidget.h"

#include <QMenu>
#include <QPainter>

#include "common/misc.h"


FileViewWidget::FileViewWidget(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::FileViewWidget)
{
	ui->setupUi(this);

	setupContextMenu();
}

FileViewWidget::~FileViewWidget()
{
	delete ui;
}

void FileViewWidget::setupContextMenu()
{
	ui->page_text->setCustomContextMenuRequestedHandler([&](){
		QMenu menu;
		QAction *a_saveas = menu.addAction("Save as...");
		QAction *a_copy = menu.addAction("Copy");
		QAction *a = menu.exec(QCursor::pos() + QPoint(8, -4));
		if (a) {
			if (a == a_saveas) {
				return;
			}
			if (a == a_copy) {
				ui->page_text->editCopy();
				return;
			}
		}
	});
}
void FileViewWidget::bind(MainWindow *mw, FileDiffWidget *fdw)
{
	ui->page_image->bind(mw, fdw);
}

void FileViewWidget::setViewType(FileViewType type)
{
	switch (type) {
	case FileViewType::Text:
		ui->stackedWidget->setCurrentWidget(ui->page_text);
		return;
	case FileViewType::Image:
		ui->stackedWidget->setCurrentWidget(ui->page_image);
		return;
	default:
		ui->stackedWidget->setCurrentWidget(ui->page_none);
		return;
	}
}

void FileViewWidget::setImage(QString mimetype, const QByteArray &ba, const QString &source_id)
{
	this->source_id = source_id;
	ui->page_image->setImage(mimetype, ba);
}

const TextEditorTheme *FileViewWidget::theme() const
{
	return ui->page_text->theme();
}

int FileViewWidget::latin1Width() const
{
	return ui->page_text->latin1Width();
}

int FileViewWidget::lineHeight() const
{
	return ui->page_text->lineHeight();
}

void FileViewWidget::setDiffMode(TextEditorEnginePtr editor_engine, QScrollBar *vsb, QScrollBar *hsb)
{
	ui->page_text->setRenderingMode(TextEditorWidget::DecoratedMode);
	ui->page_text->setTheme(TextEditorTheme::Light());
	ui->page_text->setTextEditorEngine(editor_engine);
	ui->page_text->showHeader(false);
	ui->page_text->showFooter(false);
	ui->page_text->setAutoLayout(true);
	ui->page_text->setReadOnly(true);
	ui->page_text->setToggleSelectionAnchorEnabled(false);
	ui->page_text->  setFocusFrameVisible(true);
	return ui->page_text->bindScrollBar(vsb, hsb);
}

void FileViewWidget::refrectScrollBar()
{
	return ui->page_text->refrectScrollBar();
}

void FileViewWidget::move(int cur_row, int cur_col, int scr_row, int scr_col, bool auto_scroll)
{
	return ui->page_text->move(cur_row, cur_col, scr_row, scr_col, auto_scroll);
}

void FileViewWidget::setText(const QList<Document::Line> *source, const QString &source_id)
{
	this->source_id = source_id;
	ui->page_text->setDocument(source);
}

void FileViewWidget::scrollToTop()
{
	ui->page_text->scrollToTop();
}

void FileViewWidget::write(QKeyEvent *e)
{
	ui->page_text->write(e);
}

TextEditorWidget *FileViewWidget::texteditor()
{
	return ui->page_text;
}

