#ifndef FILEHISTORYWINDOW_H
#define FILEHISTORYWINDOW_H

#include <QDialog>
#include "Git.h"
#include "MainWindow.h"
#include "FileDiffWidget.h"

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

public:
	explicit FileHistoryWindow(QWidget *parent, GitPtr g, QString const &path);
	~FileHistoryWindow();

private slots:
	void on_tableWidget_log_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);

private:
	Ui::FileHistoryWindow *ui;


	void collectFileHistory();
	void updateDiffView();
};

#endif // FILEHISTORYWINDOW_H
