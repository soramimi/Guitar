#ifndef FILEHISTORYWINDOW_H
#define FILEHISTORYWINDOW_H

#include <QDialog>
#include "Git.h"
#include "BasicMainWindow.h"
#include "FileDiffWidget.h"

namespace Ui {
class FileHistoryWindow;
}

class BasicMainWindow;
class QTableWidgetItem;

class FileHistoryWindow : public QDialog {
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
	explicit FileHistoryWindow(BasicMainWindow *parent);
	~FileHistoryWindow() override;

	void prepare(const GitPtr &g, QString const &path);
private slots:
	void on_tableWidget_log_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);

//	void onMoveNextItem();
//	void onMovePreviousItem();
	void on_tableWidget_log_customContextMenuRequested(const QPoint &pos);

private:
	Ui::FileHistoryWindow *ui;

	void collectFileHistory();
	void updateDiffView();
	BasicMainWindow *mainwindow();
};

#endif // FILEHISTORYWINDOW_H
