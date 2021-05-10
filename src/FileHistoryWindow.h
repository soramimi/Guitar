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
	struct Private;
	Private *m;

	FileDiffWidget::DiffData *diffdata();
	FileDiffWidget::DiffData const *diffdata() const;
	int totalTextLines() const;
public:
	explicit FileHistoryWindow(MainWindow *parent);
	~FileHistoryWindow() override;

	void prepare(const GitPtr &g, QString const &path);
private slots:
	void on_tableWidget_log_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);

	void on_tableWidget_log_customContextMenuRequested(const QPoint &pos);

private:
	Ui::FileHistoryWindow *ui;

	void collectFileHistory();
	void updateDiffView();
	MainWindow *mainwindow();
};

#endif // FILEHISTORYWINDOW_H
