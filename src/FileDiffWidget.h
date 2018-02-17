#ifndef FILEDIFFWIDGET_H
#define FILEDIFFWIDGET_H

#include "texteditor/AbstractCharacterBasedApplication.h"
#include <QDialog>
#include "Git.h"
#include "MainWindow.h"
#include "FileViewWidget.h"
#include "FileDiffSliderWidget.h"

namespace Ui {
class FileDiffWidget;
}

enum class ViewType {
	None,
	Left,
	Right
};

typedef Document::Line TextDiffLine;
typedef QList<Document::Line> TextDiffLineList;

struct ObjectContent {
	QString id;
	QString path;
	QByteArray bytes;
	TextDiffLineList lines;
};
typedef std::shared_ptr<ObjectContent> ObjectContentPtr;

class QTableWidgetItem;

class FileDiffWidget : public QWidget
{
	Q_OBJECT
	friend class BigDiffWindow;
public:
	struct DiffData {
		ObjectContentPtr left;
		ObjectContentPtr right;
		std::vector<std::string> original_lines;
		DiffData()
		{
			clear();
		}
		void clear()
		{
			left = ObjectContentPtr(new ObjectContent());
			right = ObjectContentPtr(new ObjectContent());
			original_lines.clear();
		}
	};

	struct DrawData {
		int v_scroll_pos = 0;
		int h_scroll_pos = 0;
		int char_width = 0;
		int line_height = 0;
		QColor bgcolor_text;
		QColor bgcolor_add;
		QColor bgcolor_del;
		QColor bgcolor_add_dark;
		QColor bgcolor_del_dark;
		QColor bgcolor_gray;
		QWidget *forcus = nullptr;
		DrawData();
	};

	enum ViewStyle {
		None,
		SingleFile,
		LeftOnly,
		RightOnly,
		SideBySideText,
		SideBySideImage,
	};

private:
	Ui::FileDiffWidget *ui;

	struct Private;
	Private *m;

	struct InitParam_ {
		ViewStyle view_style = ViewStyle::None;
		QByteArray bytes_a;
		QByteArray bytes_b;
		Git::Diff diff;
		bool uncommited = false;
		QString workingdir;
	};

	ViewStyle viewstyle() const;

	GitPtr git();
	Git::Object cat_file(GitPtr g, const QString &id);

	int totalTextLines() const;

	void resetScrollBarValue();
	void updateSliderCursor();

	int fileviewHeight() const;

	void setDiffText(const Git::Diff &diff, const TextDiffLineList &left, const TextDiffLineList &right);


	void setLeftOnly(const QByteArray &ba, const Git::Diff &diff);
	void setRightOnly(const QByteArray &ba, const Git::Diff &diff);
	void setSideBySide(const QByteArray &ba, const Git::Diff &diff, bool uncommited, const QString &workingdir);
	void setSideBySide_(const QByteArray &ba_a, const QByteArray &ba_b, const QString &workingdir);

	bool isValidID_(const QString &id);

	FileViewType setupPreviewWidget();

	void makeSideBySideDiffData(const Git::Diff &diff, const std::vector<std::string> &original_lines, TextDiffLineList *left_lines, TextDiffLineList *right_lines);
//	void setBinaryMode(bool f);
	void onUpdateSliderBar();
	void refrectScrollBar();
	void setOriginalLines_(const QByteArray &ba);
	QString diffObjects(GitPtr g, const QString &a_id, const QString &b_id);
protected:
	void resizeEvent(QResizeEvent *);
	void keyPressEvent(QKeyEvent *event);
public:
	explicit FileDiffWidget(QWidget *parent = 0);
	~FileDiffWidget();

	void bind(MainWindow *mw);

	void clearDiffView();

	void setSingleFile(QByteArray const &ba, const QString &id, const QString &path);

	void updateControls();
	void scrollToBottom();

	void updateDiffView(const Git::Diff &info, bool uncommited);
	void updateDiffView(QString id_left, QString id_right);

	void setMaximizeButtonEnabled(bool f);
	void setFocusAcceptable(bool f);
	QPixmap makeDiffPixmap(DiffPane pane, int width, int height);
	void setViewType(FileViewType type);
	void setTextCodec(QTextCodec *codec);
	void setTextCodec(const char *name);
private slots:
	void onVerticalScrollValueChanged(int);
	void onHorizontalScrollValueChanged(int);
	void onDiffWidgetWheelScroll(int lines);
	void onScrollValueChanged2(int value);
	void onDiffWidgetResized();
	void on_toolButton_fullscreen_clicked();

//	void setBinaryMode();
	void scrollTo(int value);
	void onMoved(int cur_row, int cur_col, int scr_row, int scr_col);
	void on_toolButton_menu_clicked();

signals:
	void moveNextItem();
	void movePreviousItem();
	void escPressed();
	void textcodecChanged();
};

#endif // FILEDIFFWIDGET_H
