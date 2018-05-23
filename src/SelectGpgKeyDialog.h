#ifndef SELECTGPGKEYDIALOG_H
#define SELECTGPGKEYDIALOG_H

#include <QDialog>
#include "gpg.h"

class MainWindow;
class QTableWidgetItem;

namespace Ui {
class SelectGpgKeyDialog;
}

class SelectGpgKeyDialog : public QDialog
{
	Q_OBJECT
private:
	QList<gpg::Data> keys_;
public:
	explicit SelectGpgKeyDialog(QWidget *parent, const QList<gpg::Data> &keys);
	~SelectGpgKeyDialog();

	gpg::Data key() const;

private slots:
	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);

private:
	Ui::SelectGpgKeyDialog *ui;
	void updateTable();
};

#endif // SELECTGPGKEYDIALOG_H
