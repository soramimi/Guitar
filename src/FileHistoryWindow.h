#ifndef FILEHISTORYWINDOW_H
#define FILEHISTORYWINDOW_H

#include <QDialog>
#include "Git.h"
#include "MainWindow.h"
#include "FileDiffWidget.h"

namespace Ui {
class FileHistoryWindow;
}

class MainWindow;
class QTableWidgetItem;

class FileHistoryWindow : public QDialog
{
	Q_OBJECT
private:
	struct Private;
	Private *m;

	FileDiffWidget::DiffData *diffdata();
	FileDiffWidget::DiffData const *diffdata() const;
	FileDiffWidget::DrawData *drawdata();
	FileDiffWidget::DrawData const *drawdata() const;
	int totalTextLines() const;
	int fileviewScrollPos() const;
public:
	explicit FileHistoryWindow(MainWindow *parent);
	~FileHistoryWindow();

	void prepare(GitPtr g, QString const &path);
private slots:
	void on_tableWidget_log_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);

	void onMoveNextItem();
	void onMovePreviousItem();
	void on_tableWidget_log_customContextMenuRequested(const QPoint &pos);

private:
	Ui::FileHistoryWindow *ui;

	void collectFileHistory();
	void updateDiffView();
	MainWindow *mainwindow();
};

#endif // FILEHISTORYWINDOW_H
