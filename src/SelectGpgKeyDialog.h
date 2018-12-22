#ifndef SELECTGPGKEYDIALOG_H
#define SELECTGPGKEYDIALOG_H

#include <QDialog>
#include "gpg.h"

class MainWindow;
class QTableWidgetItem;

namespace Ui {
class SelectGpgKeyDialog;
}

class SelectGpgKeyDialog : public QDialog {
	Q_OBJECT
private:
	Ui::SelectGpgKeyDialog *ui;
	QList<gpg::Data> keys_;
	void updateTable();
public:
	explicit SelectGpgKeyDialog(QWidget *parent, const QList<gpg::Data> &keys);
	~SelectGpgKeyDialog() override;

	gpg::Data key() const;

private slots:
	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);

};

#endif // SELECTGPGKEYDIALOG_H
