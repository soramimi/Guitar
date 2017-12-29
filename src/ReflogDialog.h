#ifndef REFLOGDIALOG_H
#define REFLOGDIALOG_H

#include "Git.h"

#include <QDialog>

namespace Ui {
class ReflogDialog;
}

class MainWindow;
class QTableWidgetItem;

class ReflogDialog : public QDialog
{
	Q_OBJECT
private:
	MainWindow *mainwindow_;
	Git::ReflogItemList reflog_;
public:
	explicit ReflogDialog(QWidget *parent, MainWindow *mainwin, const Git::ReflogItemList &reflog);
	~ReflogDialog();

private slots:
	void on_tableWidget_customContextMenuRequested(const QPoint &pos);

	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);

private:
	Ui::ReflogDialog *ui;
	void updateTable(const Git::ReflogItemList &reflog);
	bool currentCommit(Git::CommitItem *out);
};

#endif // REFLOGDIALOG_H
