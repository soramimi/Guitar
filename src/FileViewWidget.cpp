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

	setContextMenuPolicy(Qt::DefaultContextMenu);
//	setupContextMenu();
}

FileViewWidget::~FileViewWidget()
{
	delete ui;
}

#if 0
void FileViewWidget::setupContextMenu()
{
	ui->page_text->setCustomContextMenuRequestedHandler([&](){
		QMenu menu;
		QAction *a_save_as = menu.addAction("Save as...");
		QAction *a_copy = menu.addAction("Copy");
		QAction *a = menu.exec(this, nullptr);
//		QAction *a = menu.exec(this, misc::contextMenuPos());
		if (a) {
			if (a == a_save_as) {
				qDebug() << source_id;
				return;
			}
			if (a == a_copy) {
				ui->page_text->editCopy();
				return;
			}
		}
	});
}
#endif

void FileViewWidget::bind(MainWindow *mw, FileDiffWidget *fdw, QScrollBar *vsb, QScrollBar *hsb)
{
	ui->page_image->bind(mw, fdw, vsb, hsb);
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
	ui->page_text->setFocusFrameVisible(true);
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

void FileViewWidget::setImage(QString mimetype, const QByteArray &ba, const QString &object_id, QString const &path)
{
	setViewType(FileViewType::Image);
	this->source_id = object_id;
	ui->page_image->setImage(mimetype, ba, object_id, path);
}

void FileViewWidget::setText(const QList<Document::Line> *source, MainWindow *mw, const QString &object_id, QString const &object_path)
{
	setViewType(FileViewType::Text);
	this->source_id = object_id;
	ui->page_text->setDocument(source, mw, object_id, object_path);
	scrollToTop();
}

void FileViewWidget::setText(QByteArray const &ba, MainWindow *mw, const QString &object_id, QString const &object_path)
{
	std::vector<std::string> lines;
	char const *begin = ba.data();
	char const *end = begin + ba.size();
	misc::splitLines(begin, end, &lines, true);
	TextDiffLineList source;
	source.reserve(lines.size());
	int num = 0;
	for (std::string const &line : lines) {
		Document::Line t(line);
		t.line_number = ++num;
		source.push_back(t);
	}
	setText(&source, mw, object_id, object_path);
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

