#ifndef FILEVIEWWIDGET_H
#define FILEVIEWWIDGET_H

#include <QWidget>

#include "texteditor/TextEditorWidget.h"

class QScrollBar;
struct PreEditText;
class BasicMainWindow;
class FileDiffWidget;
class QVBoxLayout;
class QStackedWidget;

enum class FileViewType {
	None,
	Text,
	Image,
};

#ifdef APP_GUITAR
#include "MyTextEditorWidget.h"
#include "MyImageViewWidget.h"
#else
#include "ImageViewWidget.h"
#endif

class FileViewWidget : public QWidget {
private:

#ifdef APP_GUITAR
using X_TextEditorWidget = MyTextEditorWidget;
using X_ImageViewWidget = MyImageViewWidget;
#else
using X_TextEditorWidget = TextEditorWidget;
using X_ImageViewWidget = ImageViewWidget;
#endif

	QVBoxLayout *ui_verticalLayout;
	QStackedWidget *ui_stackedWidget;
	QWidget *ui_page_none;
	X_TextEditorWidget *ui_page_text;
	X_ImageViewWidget *ui_page_image;

	QString source_id;
	FileViewType view_type = FileViewType::None;

public:
	explicit FileViewWidget(QWidget *parent = nullptr);

	void setTextCodec(QTextCodec *codec);

	void setViewType(FileViewType type);

	void setImage(const QString &mimetype, const QByteArray &ba, QString const &object_id, const QString &path);
	void setText(const QList<Document::Line> *source, QMainWindow *mw, QString const &object_id, const QString &object_path);
	void setText(const QByteArray &ba, QMainWindow *mw, const QString &object_id, const QString &object_path);

	void setDiffMode(const TextEditorEnginePtr &editor_engine, QScrollBar *vsb, QScrollBar *hsb);

//	int latin1Width(const QString &s) const;
	int lineHeight() const;

	TextEditorTheme const *theme() const;
	void scrollToTop();
	void write(QKeyEvent *e);
	void refrectScrollBar();
	void move(int cur_row, int cur_col, int scr_row, int scr_col, bool auto_scroll);

	TextEditorWidget *texteditor();
	void bind(QMainWindow *mw, FileDiffWidget *fdw, QScrollBar *vsb, QScrollBar *hsb, const TextEditorThemePtr &theme);
};

#endif // FILEVIEWWIDGET_H
