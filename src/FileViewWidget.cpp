#include "FileViewWidget.h"
#include "common/misc.h"
#include <QMenu>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

FileViewWidget::FileViewWidget(QWidget *parent)
	: QWidget(parent)
{
	setObjectName(QStringLiteral("FileViewWidget"));
	ui_verticalLayout = new QVBoxLayout(this);
	ui_verticalLayout->setSpacing(0);
	ui_verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
	ui_verticalLayout->setContentsMargins(0, 0, 0, 0);
	ui_stackedWidget = new QStackedWidget(this);
	ui_stackedWidget->setObjectName(QStringLiteral("stackedWidget"));
	ui_page_none = new QWidget();
	ui_page_none->setObjectName(QStringLiteral("page_none"));
	ui_stackedWidget->addWidget(ui_page_none);
	ui_page_text = new X_TextEditorWidget();
	ui_page_text->view()->setObjectName(QStringLiteral("page_text"));
	ui_page_text->view()->setFocusPolicy(Qt::ClickFocus);
	ui_stackedWidget->addWidget(ui_page_text);
	ui_page_image = new X_ImageViewWidget();
	ui_page_image->setObjectName(QStringLiteral("page_image"));
	ui_page_image->setFocusPolicy(Qt::ClickFocus);
	ui_stackedWidget->addWidget(ui_page_image);
	ui_verticalLayout->addWidget(ui_stackedWidget);
	setWindowTitle(QApplication::translate("FileViewWidget", "Form", Q_NULLPTR));
	ui_stackedWidget->setCurrentIndex(1);
	QMetaObject::connectSlotsByName(this);

	ui_page_text->view()->setRenderingMode(TextEditorView::GraphicMode);
	ui_page_text->view()->setTheme(TextEditorTheme::Light());
	ui_page_text->view()->showHeader(false);
	ui_page_text->view()->showFooter(false);
	ui_page_text->view()->setAutoLayout(true);
	ui_page_text->view()->setReadOnly(true);
	ui_page_text->view()->setToggleSelectionAnchorEnabled(false);
	ui_page_text->view()->setFocusFrameVisible(true);

	ui_stackedWidget->setCurrentWidget(ui_page_none);
}

void FileViewWidget::setTextCodec(QTextCodec *codec)
{
	ui_page_text->view()->setTextCodec(codec);
}

void FileViewWidget::bind(QMainWindow *mw, FileDiffWidget *fdw, QScrollBar *vsb, QScrollBar *hsb, TextEditorThemePtr const &theme)
{
	ui_page_text->view()->bindScrollBar(vsb, hsb);
	ui_page_image->bind(mw, fdw, vsb, hsb);
	ui_page_text->view()->setTheme(theme);
}

void FileViewWidget::setViewType(FileViewType type)
{
	view_type = type;
	switch (view_type) {
	case FileViewType::Text:
		ui_stackedWidget->setCurrentWidget(ui_page_text);
		return;
	case FileViewType::Image:
		ui_stackedWidget->setCurrentWidget(ui_page_image);
		return;
	default:
		ui_stackedWidget->setCurrentWidget(ui_page_none);
		return;
	}
}

const TextEditorTheme *FileViewWidget::theme() const
{
	return ui_page_text->view()->theme();
}

//int FileViewWidget::latin1Width(QString const &s) const
//{
//	return ui_page_text->view()->latin1Width(s);
//}

int FileViewWidget::lineHeight() const
{
	return ui_page_text->view()->lineHeight();
}

void FileViewWidget::setDiffMode(TextEditorEnginePtr const &editor_engine, QScrollBar *vsb, QScrollBar *hsb)
{
	ui_page_text->view()->setTextEditorEngine(editor_engine);
	return ui_page_text->view()->bindScrollBar(vsb, hsb);
}

void FileViewWidget::refrectScrollBar()
{
	switch (view_type) {
	case FileViewType::Text:
		ui_page_text->view()->refrectScrollBar();
		return;
	case FileViewType::Image:
		ui_page_image->refrectScrollBar();
		return;
	}
}

void FileViewWidget::move(int cur_row, int cur_col, int scr_row, int scr_col, bool auto_scroll)
{
	return ui_page_text->view()->move(cur_row, cur_col, scr_row, scr_col, auto_scroll);
}

void FileViewWidget::clear()
{
	setViewType(FileViewType::None);
	this->source_id.clear();
#ifdef APP_GUITAR
	ui_page_text->clear();
	scrollToTop();
	texteditor()->moveCursorOut();
#else
	ui_page_text->view()->setDocument(nullptr);
	scrollToTop();
#endif
}

void FileViewWidget::setImage(QString const &mimetype, QByteArray const &ba, QString const &object_id, QString const &path)
{
	setViewType(FileViewType::Image);
	this->source_id = object_id;
#ifdef APP_GUITAR
	ui_page_image->setImage(mimetype, ba, object_id, path);
#else
	ui_page_image->setImage(mimetype, ba);
#endif
}

void FileViewWidget::setText(const QList<Document::Line> *source, QMainWindow *mw, QString const &object_id, QString const &object_path)
{
	setViewType(FileViewType::Text);
	this->source_id = object_id;
#ifdef APP_GUITAR
	ui_page_text->setDocument(source, qobject_cast<MainWindow *>(mw), object_id, object_path);
	scrollToTop();
	texteditor()->moveCursorOut(); // 現在行を -1 にして、カーソルを非表示にする。
#else
	ui_page_text->view()->setDocument(source);
	scrollToTop();
#endif
}

void FileViewWidget::setText(QByteArray const &ba, QMainWindow *mw, QString const &object_id, QString const &object_path)
{
	std::vector<std::string> lines;
	char const *begin = ba.data();
	char const *end = begin + ba.size();
	misc::splitLines(begin, end, &lines, true);
	QList<Document::Line> source;
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
	ui_page_text->view()->scrollToTop();
}

void FileViewWidget::write(QKeyEvent *e)
{
	ui_page_text->view()->write(e);
}

TextEditorView *FileViewWidget::texteditor()
{
	return ui_page_text->view();
}

