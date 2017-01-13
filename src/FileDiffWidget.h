#ifndef FILEDIFFWIDGET_H
#define FILEDIFFWIDGET_H

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

struct TextDiffLine {
	enum Type {
		Unknown,
		Unchanged,
		Add,
		Del,
	} type;
	int hunk_number = -1;
	int line_number = -1;
	QString line;
	TextDiffLine()
	{
	}
	TextDiffLine(QString const &text)
		: line(text)
	{
	}
};

struct DiffWidgetData {
	struct DiffData {
		QStringList original_lines;
		QList<TextDiffLine> left_lines;
		QList<TextDiffLine> right_lines;
		QString path;
		Git::BLOB left;
		Git::BLOB right;
	} diffdata;
	struct DrawData {
		int scrollpos = 0;
		int char_width = 0;
		int line_height = 0;
		QColor bgcolor_text;
		QColor bgcolor_add;
		QColor bgcolor_del;
		QColor bgcolor_add_dark;
		QColor bgcolor_del_dark;
		QColor bgcolor_gray;
		ViewType forcus = ViewType::None;
		DrawData();
	} drawdata;
};


class QTableWidgetItem;

class FileDiffWidget : public QWidget
{
	Q_OBJECT
	friend class MainWindow;
	friend class FileDiffSliderWidget;
private:
	MainWindow *mainwindow;
	QString path;
	Git::CommitItemList commit_item_list;

	DiffWidgetData data_;

	DiffWidgetData::DiffData *diffdata()
	{
		return &data_.diffdata;
	}

	DiffWidgetData::DiffData const *diffdata() const
	{
		return &data_.diffdata;
	}

	DiffWidgetData::DrawData *drawdata()
	{
		return &data_.drawdata;
	}

	DiffWidgetData::DrawData const *drawdata() const
	{
		return &data_.drawdata;
	}

	int totalTextLines() const
	{
		return diffdata()->left_lines.size();
	}

	int fileviewScrollPos() const
	{
		return drawdata()->scrollpos;
	}

	int visibleLines() const
	{
		int n = 0;
		if (drawdata()->line_height > 0) {
			n = fileviewHeight() / drawdata()->line_height;
			if (n < 1) n = 1;
		}
		return n;
	}

	void scrollTo(int value);

public:
	explicit FileDiffWidget(QWidget *parent = 0);
	~FileDiffWidget();



	DiffWidgetData *getDiffWidgetData()
	{
		return &data_;
	}
	void bind(MainWindow *mw);
	void setPath(const QString &path);
private slots:




	void onScrollValueChanged(int value);

	void onDiffWidgetWheelScroll(int lines);

	void onScrollValueChanged2(int value);

	void onDiffWidgetResized();



private:
	Ui::FileDiffWidget *ui;


	void updateVerticalScrollBar();
	int fileviewHeight() const;
	QString formatLine(QString const &text, bool diffmode);

	void setDiffText_(const QList<TextDiffLine> &left, const QList<TextDiffLine> &right, bool diffmode);

	void setDataAsNewFile(const QByteArray &ba, const Git::Diff &diff);
	void setTextDiffData(const QByteArray &ba, const Git::Diff &diff, bool uncommited, const QString &workingdir);

	void init_diff_data_(const Git::Diff &diff);

	GitPtr git();

	QPixmap makeDiffPixmap_(ViewType side, int width, int height, const DiffWidgetData *dd);
	QPixmap makeDiffPixmap(ViewType side, int width, int height);
public:
	bool eventFilter(QObject *watched, QEvent *event);

	void setVerticalScrollBarValue(int pos);
	void clearDiffView();
	void updateSliderCursor();

	void updateDiffView(const Git::Diff &info, bool uncommited);
	void updateDiffView(QString id_left, QString id_right);
};

#endif // FILEDIFFWIDGET_H
