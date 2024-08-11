#ifndef FILEDIFFWIDGET_H
#define FILEDIFFWIDGET_H

#include "FileDiffSliderWidget.h"
#include "FileViewWidget.h"
#include "Git.h"
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

class MainWindow;

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

/**
 * @brief The FileDiffWidget class
 * サイドバイサイドで2つのファイルのdiffを表示するウィジェット
 */
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

	enum ViewStyle {
		None,
		SingleFile,
		LeftOnly,
		RightOnly,
		SideBySideText,
		SideBySideImage,
	};

	struct LineFragment {
		Document::Line::Type type = Document::Line::Unknown;
		int line_index;
		int line_count;
		LineFragment() = default;
		LineFragment(Document::Line::Type type, int line_index, int line_count)
			: type(type)
			, line_index(line_index)
			, line_count(line_count)
		{
		}
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

	static MainWindow *mainwindow();

	ViewStyle viewstyle() const;

	static GitPtr git();
	static Git::Object catFile(QString const &id);

	int totalTextLines() const;

	void resetScrollBarValue();
	void updateSliderCursor();

	int fileviewHeight() const;

	void setDiffText(const Git::Diff &diff, TextDiffLineList const &left, TextDiffLineList const &right);


	void setLeftOnly(const Git::Diff &diff, QByteArray const &ba);
	void setRightOnly(const Git::Diff &diff, QByteArray const &ba);
	void setSideBySide(const Git::Diff &diff, QByteArray const &ba, bool uncommited, QString const &workingdir);
	void setSideBySide_(const Git::Diff &diff, QByteArray const &ba_a, QByteArray const &ba_b, QString const &workingdir);

	static bool isValidID_(QString const &id);

	FileViewType setupPreviewWidget();

	void makeSideBySideDiffData(const Git::Diff &diff, const std::vector<std::string> &original_lines, TextDiffLineList *left_lines, TextDiffLineList *right_lines);
	void onUpdateSliderBar();
	void refrectScrollBar();
	void refrectScrollBarV();
	void refrectScrollBarH();
	void setOriginalLines_(QByteArray const &ba, const Git::SubmoduleItem *submodule, const Git::CommitItem *submodule_commit);
	QString diffObjects(QString const &a_id, QString const &b_id);
	bool setSubmodule(const Git::Diff &diff);
protected:
	void resizeEvent(QResizeEvent *) override;
	void keyPressEvent(QKeyEvent *event) override;
public:
	explicit FileDiffWidget(QWidget *parent = nullptr);
	~FileDiffWidget() override;

	void init();

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
