#ifndef FILEDIFFWIDGET_H
#define FILEDIFFWIDGET_H

#include <AbstractCharacterBasedApplication.h>
#include <QDialog>
#include "Git.h"
#include "MainWindow.h"

namespace Ui {
class FileDiffWidget;
}

enum class ViewType {
	None,
	Left,
	Right
};

enum class FilePreviewType {
	None,
	Text,
	Image,
};

//struct TextDiffLine {
//	enum Type {
//		Unknown,
//		Normal,
//		Add,
//		Del,
//	} type = Unknown;
//	int hunk_number = -1;
//	int line_number = 0;
//	QByteArray text;
//	TextDiffLine(Type type = Unknown, size_t reserve_length = 0)
//		: type(type)
//	{
//		text.reserve(reserve_length);
//	}
//	TextDiffLine(std::string const &text_, Type type)
//		: type(type)
//	{
//		text.append(text_.c_str(), text_.size());
//	}
//};
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
	enum class Pane {
		Left,
		Right,
	};
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
		Terminal,
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

//	FileDiffWidget::DiffData *diffdata();
//	FileDiffWidget::DiffData const *diffdata() const;
//	FileDiffWidget::DrawData *drawdata();
//	FileDiffWidget::DrawData const *drawdata() const;

	ViewStyle viewstyle() const;

	GitPtr git();
	Git::Object cat_file(GitPtr g, const QString &id);

	int totalTextLines() const;

//	int fileviewScrollPos() const;

	int visibleLines() const;

	void resetScrollBarValue();
	void updateVerticalScrollBar();
	void updateHorizontalScrollBar();
	void updateSliderCursor();

	int fileviewHeight() const;

	void setDiffText(const Git::Diff &diff, const std::vector<std::string> &original_lines, const TextDiffLineList &left, const TextDiffLineList &right);


	void setLeftOnly(const QByteArray &ba, const Git::Diff &diff);
	void setRightOnly(const QByteArray &ba, const Git::Diff &diff);
	void setSideBySide(const QByteArray &ba, const Git::Diff &diff, bool uncommited, const QString &workingdir);
	void setSideBySide(const QByteArray &ba_a, const QByteArray &ba_b, const QString &workingdir);

	bool isValidID_(const QString &id);

	FilePreviewType setupPreviewWidget();

	void makeSideBySideDiffData(const Git::Diff &diff, const std::vector<std::string> &original_lines, TextDiffLineList *left_lines, TextDiffLineList *right_lines);
	void setBinaryMode(bool f);
	void bindContent_();
	bool isTerminalMode() const;
	void onUpdateSliderBar();
	void refrectScrollBar();
	void setOriginalLines_(const QByteArray &ba);
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

	QPixmap makeDiffPixmap(ViewType side, int width, int height, const DiffData *diffdata, const FileDiffWidget::DrawData *drawdata);

	void setMaximizeButtonEnabled(bool f);
	void setLeftBorderVisible(bool f);
	void termWrite(ushort c);
	void termWrite(const ushort *begin, const ushort *end);
	void termWrite(const QString &text);
	void setTerminalMode();
	void setFocusAcceptable(bool f);
	QPixmap makeDiffPixmap(Pane pane, int width, int height);
private slots:
	void onVerticalScrollValueChanged(int value);
	void onHorizontalScrollValueChanged(int value);
	void onDiffWidgetWheelScroll(int lines);
	void onScrollValueChanged2(int value);
	void onDiffWidgetResized();
	void on_toolButton_fullscreen_clicked();

	void setBinaryMode();
	void scrollTo(int value);
	void onMoved(int cur_row, int cur_col, int scr_row, int scr_col);
protected:
	bool eventFilter(QObject *watched, QEvent *event);
signals:
	void moveNextItem();
	void movePreviousItem();

	// QWidget interface
protected:
	void resizeEvent(QResizeEvent *event);

	// QWidget interface
protected:
	void keyPressEvent(QKeyEvent *event);
};

#endif // FILEDIFFWIDGET_H
