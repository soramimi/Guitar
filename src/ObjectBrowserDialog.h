#ifndef OBJECTBROWSERDIALOG_H
#define OBJECTBROWSERDIALOG_H

#include <QDialog>
#include "Git.h"

class BasicMainWindow;
class QListWidgetItem;
class QTableWidgetItem;

namespace Ui {
class ObjectBrowserDialog;
}

class ObjectBrowserDialog : public QDialog {
	Q_OBJECT
private:
	Ui::ObjectBrowserDialog *ui;
	BasicMainWindow *mainwindow();
	GitPtr git();
public:
	explicit ObjectBrowserDialog(BasicMainWindow *parent, QStringList const &list);
	~ObjectBrowserDialog();
	QString text() const;
private slots:
	void on_pushButton_inspect_clicked();
	void on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);
};

#endif // OBJECTBROWSERDIALOG_H
