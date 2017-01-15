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
	struct Private;
	Private *pv;

	FileDiffWidget::DiffData *diffdata();
	FileDiffWidget::DiffData const *diffdata() const;
	FileDiffWidget::DrawData *drawdata();
	FileDiffWidget::DrawData const *drawdata() const;
	int totalTextLines() const;
	int fileviewScrollPos() const;
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
