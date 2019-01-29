#ifndef JUMPDIALOG_H
#define JUMPDIALOG_H

#include "MyTableWidgetDelegate.h"
#include "Git.h"
#include <QDialog>

namespace Ui {
class JumpDialog;
}

class QTableWidgetItem;

class JumpDialog : public QDialog {
	Q_OBJECT
public:
private:
	struct Private;
	Private *m;
public:
	explicit JumpDialog(QWidget *parent, NamedCommitList const &items);
	~JumpDialog() override;
	QString text() const;
	static void sort(NamedCommitList *items);
private slots:
	void on_toolButton_clicked();
	void on_lineEdit_filter_textChanged(QString const &text);
	void on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
private:
	Ui::JumpDialog *ui;
	void updateTable();
	void internalUpdateTable(const NamedCommitList &list2);
};

#endif // JUMPDIALOG_H
