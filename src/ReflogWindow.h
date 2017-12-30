#ifndef REFLOGWINDOW_H
#define REFLOGWINDOW_H

#include "Git.h"

#include <QDialog>

namespace Ui {
class ReflogWindow;
}

class MainWindow;
class QTableWidgetItem;

class ReflogWindow : public QDialog
{
	Q_OBJECT
private:
	MainWindow *mainwindow_;
	Git::ReflogItemList reflog_;
public:
	explicit ReflogWindow(QWidget *parent, MainWindow *mainwin, const Git::ReflogItemList &reflog);
	~ReflogWindow();

private slots:
	void on_tableWidget_customContextMenuRequested(const QPoint &pos);

	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);

private:
	Ui::ReflogWindow *ui;
	void updateTable(const Git::ReflogItemList &reflog);
	bool currentCommit(Git::CommitItem *out);
};

#endif // REFLOGWINDOW_H
