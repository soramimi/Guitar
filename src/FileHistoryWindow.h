#ifndef FILEHISTORYWINDOW_H
#define FILEHISTORYWINDOW_H

#include <QDialog>
#include "Git.h"
#include "FileDiffWidget.h"

namespace Ui {
class FileHistoryWindow;
}

class QTableWidgetItem;

class FileHistoryWindow : public QDialog {
	Q_OBJECT
private:
	Ui::FileHistoryWindow *ui;
	struct Private;
	Private *m;
	
	MainWindow *mainwindow();
	FileDiffWidget::DiffData *diffdata();
	FileDiffWidget::DiffData const *diffdata() const;
	int totalTextLines() const;
	void collectFileHistory();
	void updateDiffView();
public:
	explicit FileHistoryWindow(MainWindow *parent);
	~FileHistoryWindow() override;
	
	void prepare(GitRunner g, QString const &path);
private slots:
	void on_tableWidget_log_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
	void on_tableWidget_log_customContextMenuRequested(const QPoint &pos);
};

#endif // FILEHISTORYWINDOW_H
