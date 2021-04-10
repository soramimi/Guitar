#ifndef OBJECTBROWSERDIALOG_H
#define OBJECTBROWSERDIALOG_H

#include <QDialog>
#include "Git.h"

class MainWindow;
class QListWidgetItem;
class QTableWidgetItem;

namespace Ui {
class ObjectBrowserDialog;
}

class ObjectBrowserDialog : public QDialog {
	Q_OBJECT
private:
	Ui::ObjectBrowserDialog *ui;
	MainWindow *mainwindow();
	GitPtr git();
public:
	explicit ObjectBrowserDialog(MainWindow *parent, QStringList const &list);
	~ObjectBrowserDialog() override;
	QString text() const;
private slots:
	void on_pushButton_inspect_clicked();
	void on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);
};

#endif // OBJECTBROWSERDIALOG_H
