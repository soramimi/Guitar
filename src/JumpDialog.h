#ifndef JUMPDIALOG_H
#define JUMPDIALOG_H

#include "CommitLogTableWidget.h"
#include "Git.h"
#include <QDialog>

namespace Ui {
class JumpDialog;
}

class QTableWidgetItem;

class JumpDialog : public QDialog {
	Q_OBJECT
private:
	Ui::JumpDialog *ui;
	struct Private;
	Private *m;
	void updateTable();
public:
	explicit JumpDialog(QWidget *parent, NamedCommitList const &items, CommitRecords commit_records);
	~JumpDialog() override;
	QString text() const;
	static void sort(NamedCommitList *items);
private slots:
	void on_lineEdit_filter_textChanged(QString const &text);
	void on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
	void on_pushButton_checkout_clicked();
};

#endif // JUMPDIALOG_H
