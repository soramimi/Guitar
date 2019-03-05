#ifndef FILEDIFFWIDGET_H
#define FILEDIFFWIDGET_H

#include "FileDiffSliderWidget.h"
#include "FileViewWidget.h"
#include "Git.h"
#include "MainWindow.h"
#include "texteditor/AbstractCharacterBasedApplication.h"
#include <QDialog>
#include <memory>

namespace Ui {
class FileDiffWidget;
}

enum class ViewType {
	None,
	Left,
	Right
};

using TextDiffLine = Document::Line;
using TextDiffLineList = QList<Document::Line>;

struct ObjectContent {
	QString id;
	QString path;
	QByteArray bytes;
	TextDiffLineList lines;
};
using ObjectContentPtr = std::shared_ptr<ObjectContent>;

class QTableWidgetItem;

class FileDiffWidget : public QWidget {
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
			left = std::make_shared<ObjectContent>();
			right = std::make_shared<ObjectContent>();
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
	Git::Object cat_file(const GitPtr &g, QString const &id);

	int totalTextLines() const;

	void resetScrollBarValue();
	void updateSliderCursor();

	int fileviewHeight() const;

	void setDiffText(const Git::Diff &diff, TextDiffLineList const &left, TextDiffLineList const &right);


	void setLeftOnly(QByteArray const &ba, const Git::Diff &diff);
	void setRightOnly(QByteArray const &ba, const Git::Diff &diff);
	void setSideBySide(QByteArray const &ba, const Git::Diff &diff, bool uncommited, QString const &workingdir);
	void setSideBySide_(QByteArray const &ba_a, QByteArray const &ba_b, QString const &workingdir);

	bool isValidID_(QString const &id);

	FileViewType setupPreviewWidget();

	void makeSideBySideDiffData(const Git::Diff &diff, const std::vector<std::string> &original_lines, TextDiffLineList *left_lines, TextDiffLineList *right_lines);
	void onUpdateSliderBar();
	void refrectScrollBar();
	void setOriginalLines_(QByteArray const &ba);
	QString diffObjects(const GitPtr &g, QString const &a_id, QString const &b_id);
	BasicMainWindow *mainwindow();
protected:
	void resizeEvent(QResizeEvent *) override;
	void keyPressEvent(QKeyEvent *event) override;
public:
	explicit FileDiffWidget(QWidget *parent = nullptr);
	~FileDiffWidget() override;

	void bind(BasicMainWindow *mw);

	void clearDiffView();

	void setSingleFile(QByteArray const &ba, QString const &id, QString const &path);

	void updateControls();
	void scrollToBottom();

	void updateDiffView(const Git::Diff &info, bool uncommited);
	void updateDiffView(const QString &id_left, const QString &id_right, QString const &path = QString());

	void setMaximizeButtonEnabled(bool f);
	void setFocusAcceptable(Qt::FocusPolicy focuspolicy);
	QPixmap makeDiffPixmap(DiffPane pane, int width, int height);
	void setViewType(FileViewType type);
	void setTextCodec(QTextCodec *codec);
	void setTextCodec(char const *name);
private slots:
	void onVerticalScrollValueChanged(int);
	void onHorizontalScrollValueChanged(int);
	void onDiffWidgetWheelScroll(int lines);
	void onScrollValueChanged2(int value);
	void onDiffWidgetResized();
	void on_toolButton_fullscreen_clicked();

	void scrollTo(int value);
	void onMoved(int cur_row, int cur_col, int scr_row, int scr_col);
	void on_toolButton_menu_clicked();

signals:
//	void moveNextItem();
//	void movePreviousItem();
	void textcodecChanged();
};

#endif // FILEDIFFWIDGET_H
