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

	texteditor()->setTheme(TextEditorTheme::Light());
	texteditor()->showHeader(false);
	texteditor()->showFooter(false);
	texteditor()->setAutoLayout(true);
	texteditor()->setReadOnly(true);
	texteditor()->setToggleSelectionAnchorEnabled(false);
	texteditor()->setFocusFrameVisible(true);

	ui_stackedWidget->setCurrentWidget(ui_page_none);
}

void FileViewWidget::setTextCodec(QTextCodec *codec)
{
	texteditor()->setTextCodec(codec);
}

void FileViewWidget::bind(FileDiffWidget *fdw, QScrollBar *vsb, QScrollBar *hsb, TextEditorThemePtr const &theme)
{
	texteditor()->bindScrollBar(vsb, hsb);
	ui_page_image->bind(fdw, vsb, hsb);
	texteditor()->setTheme(theme);
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
	return texteditor()->theme();
}

int FileViewWidget::lineHeight() const
{
	return texteditor()->lineHeight();
}

void FileViewWidget::setDiffMode(TextEditorEnginePtr const &editor_engine, QScrollBar *vsb, QScrollBar *hsb)
{
	texteditor()->setTextEditorEngine(editor_engine);
	return texteditor()->bindScrollBar(vsb, hsb);
}

void FileViewWidget::refrectScrollBar()
{
	switch (view_type) {
	case FileViewType::Text:
		texteditor()->refrectScrollBar();
		texteditor()->fetchLines(); // スクロールバーの値に合わせて、テキスト領域をスクロールする
		return;
	case FileViewType::Image:
		ui_page_image->refrectScrollBar();
		return;
	}
}

void FileViewWidget::move(int cur_row, int cur_col, int scr_row, int scr_col, bool auto_scroll)
{
	return texteditor()->move(cur_row, cur_col, scr_row, scr_col, auto_scroll);
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
	texteditor()->setDocument(nullptr);
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

void FileViewWidget::setText(const QList<Document::Line> *source, QString const &object_id, QString const &object_path)
{
	setViewType(FileViewType::Text);
	this->source_id = object_id;
#ifdef APP_GUITAR
	ui_page_text->setDocument(source, object_id, object_path);
	scrollToTop();
	texteditor()->moveCursorOut(); // 現在行を -1 にして、カーソルを非表示にする。
#else
	texteditor()->setDocument(source);
	scrollToTop();
#endif
}

void FileViewWidget::setText(QByteArray const &ba, QString const &object_id, QString const &object_path)
{
	std::vector<std::string_view> lines = misc::splitLines(std::string_view{ba.data(), (size_t)ba.size()}, true);
	QList<Document::Line> source;
	source.reserve(lines.size());
	int num = 0;
	for (std::string_view const &line : lines) {
		Document::Line t(std::string{line});
		t.line_number = ++num;
		source.push_back(t);
	}
	setText(&source, object_id, object_path);
}

void FileViewWidget::scrollToTop()
{
	texteditor()->scrollToTop();
}

void FileViewWidget::write(QKeyEvent *e)
{
	texteditor()->write(e);
}

TextEditorView *FileViewWidget::texteditor()
{
	return ui_page_text->view();
}

TextEditorView const *FileViewWidget::texteditor() const
{
	return ui_page_text->view();
}

