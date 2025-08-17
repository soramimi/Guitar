#ifndef REFLOGWINDOW_H
#define REFLOGWINDOW_H

#include "Git.h"

#include <QDialog>

namespace Ui {
class ReflogWindow;
}

class MainWindow;
class QTableWidgetItem;

class ReflogWindow : public QDialog {
	Q_OBJECT
private:
	Ui::ReflogWindow *ui;
	MainWindow *mainwindow_;
	Git::ReflogItemList reflog_;

	MainWindow *mainwindow()
	{
		return mainwindow_;
	}

	void updateTable(const Git::ReflogItemList &reflog);
	std::optional<GitCommitItem> currentCommitItem();

public:
	explicit ReflogWindow(QWidget *parent, MainWindow *mainwin, const Git::ReflogItemList &reflog);
	~ReflogWindow() override;

private slots:
	void on_tableWidget_customContextMenuRequested(const QPoint &pos);
	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);
};

#endif // REFLOGWINDOW_H
