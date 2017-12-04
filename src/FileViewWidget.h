#ifndef FILEVIEWWIDGET_H
#define FILEVIEWWIDGET_H

#include <QWidget>

#include "texteditor/TextEditorWidget.h"

class QScrollBar;
struct PreEditText;
class MainWindow;
class FileDiffWidget;

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
	Ui::FileViewWidget *ui;
	QString source_id;
	FileViewType view_type = FileViewType::None;

//	void setupContextMenu();
public:
	explicit FileViewWidget(QWidget *parent = 0);
	~FileViewWidget();

	void setViewType(FileViewType type);

	void setImage(QString mimetype, const QByteArray &ba, QString const &object_id, const QString &path);
	void setText(const QList<Document::Line> *source, MainWindow *mw, QString const &object_id, const QString &object_path);
	void setText(const QByteArray &ba, MainWindow *mw, const QString &object_id, const QString &object_path);

	void setDiffMode(TextEditorEnginePtr editor_engine, QScrollBar *vsb, QScrollBar *hsb);

	int latin1Width() const;
	int lineHeight() const;

	TextEditorTheme const *theme() const;
	void scrollToTop();
	void write(QKeyEvent *e);
	void refrectScrollBar();
	void move(int cur_row, int cur_col, int scr_row, int scr_col, bool auto_scroll);

	TextEditorWidget *texteditor();
	void bind(MainWindow *mw, FileDiffWidget *fdw, QScrollBar *vsb, QScrollBar *hsb);
};

#endif // FILEVIEWWIDGET_H
