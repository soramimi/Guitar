#ifndef FILEVIEWWIDGET_H
#define FILEVIEWWIDGET_H

#include <QWidget>

#include "TextEditorWidget.h"

class QScrollBar;
struct PreEditText;
class MainWindow;

namespace Ui {
class FileViewWidget;
}

enum class FileViewType {
	None,
	Text,
	Image,
};

class FileViewWidget : public QWidget
{
	Q_OBJECT
private:
	QString source_id;
public:
	explicit FileViewWidget(QWidget *parent = 0);
	~FileViewWidget();

	void setViewType(FileViewType type);

	void setImage(QString mimetype, const QByteArray &ba, QString const &source_id);
	void setText(const QList<Document::Line> *source, QString const &source_id);

	void setDiffMode(TextEditorEnginePtr editor_engine, QScrollBar *vsb, QScrollBar *hsb);

	int latin1Width() const;
	int lineHeight() const;

	TextEditorTheme const *theme() const;
	void scrollToTop();
	void write(QKeyEvent *e);
	void refrectScrollBar();
	void move(int cur_row, int cur_col, int scr_row, int scr_col, bool auto_scroll);

	TextEditorWidget *texteditor();
	void bind(MainWindow *mw);
private:
	Ui::FileViewWidget *ui;
	void setupContextMenu();
};

#endif // FILEVIEWWIDGET_H
