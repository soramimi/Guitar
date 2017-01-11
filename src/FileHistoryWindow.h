#ifndef FILEHISTORYWINDOW_H
#define FILEHISTORYWINDOW_H

#include <QDialog>
#include "Git.h"
#include "MainWindow.h"

namespace Ui {
class FileHistoryWindow;
}

class QTableWidgetItem;

class FileHistoryWindow : public QDialog
{
	Q_OBJECT
private:
	MainWindow *mainwindow;
	GitPtr g;
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
	explicit FileHistoryWindow(QWidget *parent, GitPtr g, QString const &path);
	~FileHistoryWindow();

private slots:
	void on_tableWidget_log_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);




	void onScrollValueChanged(int value);

	void onDiffWidgetWheelScroll(int lines);

	void onScrollValueChanged2(int value);

	void onDiffWidgetResized();



private:
	Ui::FileHistoryWindow *ui;


	void collectFileHistory();
	void updateDiffView();
	void clearDiffView();
	void updateVerticalScrollBar();
	int fileviewHeight() const;
	QString formatLine(QString const &text, bool diffmode);

	void setDiffText_(const QList<TextDiffLine> &left, const QList<TextDiffLine> &right, bool diffmode);
	void setTextDiffData(const QByteArray &ba, const Git::Diff &diff, bool uncommited, const QString &workingdir);
	void init_diff_data_(const Git::Diff &diff);
	void updateSliderCursor();
};

#endif // FILEHISTORYWINDOW_H
